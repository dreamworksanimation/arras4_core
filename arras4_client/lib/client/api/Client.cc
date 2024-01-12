// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Client.h"
#include "ClientException.h"
#include "SessionDefinition.h"
#include "Component.h"

#include <message_api/Message.h>
#include <message_api/ArrasTime.h>

#include <core_messages/SessionStatusMessage.h>
#include <core_messages/EngineReadyMessage.h>
#include <core_messages/ControlMessage.h>
#include <core_messages/ExecutorHeartbeat.h>

#include <message_impl/Envelope.h>
#include <message_impl/PeerMessageEndpoint.h>

#include <chunking/ChunkingMessageEndpoint.h>

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <exceptions/ShutdownException.h>

#include <network/InetSocketPeer.h>
#include <network/PeerException.h>
#include <network/Buffer.h>

#include <client/local/LocalSession.h>
#include <client/local/SessionNotification.h>

#include <http/http_types.h>
#include <http/HttpResponse.h>
#include <http/HttpRequest.h>

#include <shared_impl/Platform.h>

#include <boost/filesystem.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib> // std::getenv
#include <sstream>
#include <fstream>

#if (!defined(ARRAS_WINDOWS))
#include <unistd.h> // usleep
#endif


// HTTP Header explicitly used for specifying the client version to the node websocket connection
#define HTTP_ARRAS_MESSAGING_API_VERSION "X-Arras-Messaging-API-Version"
#define HTTP_ARRAS_LISTEN_TO  "Arras-Listen-To"

#define ARRAS_MESSAGING_API_VERSION_MAJOR 4
#define ARRAS_MESSAGING_API_VERSION_MINOR 0
#define ARRAS_MESSAGING_API_VERSION_PATCH 0

namespace {
    const char* DWA_CONFIG_ENV_NAME = "DWA_CONFIG_SERVICE";
    const char* ARRAS_CONFIG_PATH = "/serve/jose/arras/endpoints/";
    const char* COORDINATOR_CONFIG_ENDPOINT = "/coordinator/url";
    const char* SESSIONS_PATH = "/sessions";
    const char* DEFAULT_LOCAL_COORDINATOR_ENDPOINT = "http://localhost:8087/coordinator/1";
    const char* LOCAL_SESSION_URL = "arras:local";

    // local mode writes a 1 line session log to $HOME/<LOCAL_LOG_DIR>/<LOCAL_LOG_NAME>
    const char* LOCAL_LOG_DIR = ".arras";
    const char* LOCAL_LOG_NAME = "localsessions";

    // Client override environment variables
    // if these are set, they override normal client behavior
    const char* ENV_OVR_COORDINATOR_URL = "ARRASCLIENT_OVR_COORDINATOR_URL";
    const char* ENV_OVR_READY_WAIT_SECS = "ARRASCLIENT_OVR_READY_WAIT_SECS";
    const char* ENV_OVR_DISCONNECT_WAIT_SECS = "ARRASCLIENT_OVR_DISCONNECT_WAIT_SECS";
    const char* ENV_OVR_CLIENT_LOG_LEVEL = "ARRASCLIENT_OVR_CLIENT_LOG_LEVEL";
    const char* ENV_OVR_CLIENT_TRACE_LEVEL = "ARRASCLIENT_OVR_CLIENT_TRACE_LEVEL";
    const char* ENV_OVR_FORCE_LOCAL_MODE = "ARRASCLIENT_OVR_FORCE_LOCAL_MODE";


    
    void applyLoggingOverrides()
    {
	const char* ovrLog = getenv(ENV_OVR_CLIENT_LOG_LEVEL);
	if (ovrLog) {
	    try {
		unsigned long logLvl = std::stoul(ovrLog);
		ARRAS_WARN(arras4::log::Id("ClientOverrideActive") <<
			   "Client override: " << ENV_OVR_CLIENT_LOG_LEVEL << " = " << logLvl);
		arras4::log::Logger::instance().setThreshold(static_cast<arras4::log::Logger::Level>(logLvl));
	    } catch (std::exception&) {
		ARRAS_ERROR(arras4::log::Id("ClientOverrideFailed") <<
			    "Invalid client override ignored: " << ENV_OVR_CLIENT_LOG_LEVEL
			    << " = " << ovrLog);
	    }
	}

	const char* ovrTrace = getenv(ENV_OVR_CLIENT_TRACE_LEVEL);
	if (ovrTrace) {
	    try {
		unsigned long traceLvl = strtoul(ovrTrace,nullptr,10);
		ARRAS_WARN(arras4::log::Id("ClientOverrideActive") <<
			   "Client override: " << ENV_OVR_CLIENT_TRACE_LEVEL << " = " << traceLvl);
		arras4::log::Logger::instance().setTraceThreshold(static_cast<int>(traceLvl));
	    } catch (std::exception&) {
		ARRAS_ERROR(arras4::log::Id("ClientOverrideFailed") <<
			    "Invalid client override ignored: " << ENV_OVR_CLIENT_TRACE_LEVEL
			    << " = " << ovrTrace);
	    }
	}
    }

    std::string getClientVersion()
    {
	std::string ret("arras4_client-");
	char *v = getenv("REZ_ARRAS4_CLIENT_VERSION");
	if (v) ret += std::string(v);
	else ret += "???";
	ret += ";arras4_core_impl-";
	v = getenv("REZ_ARRAS4_CORE_IMPL_VERSION");
	if (v) ret += std::string(v);
	else ret += "???";
	ret += ";arras4_network-";
	v = getenv("REZ_ARRAS4_NETWORK_VERSION");
	if (v) ret += std::string(v);
	else ret += "???";
	return ret;
    }

} // namespace


using namespace arras4::api;
using namespace arras4::network;
using namespace arras4::impl;

namespace arras4 {
    namespace client {

/* static */ ProcessManager& Client::processManager()
{
    static ProcessManager pm(0,false,false,false,false);
    return pm;
}

/* static */ LocalSessions& Client::localSessions()
{
    static LocalSessions ls(processManager());
    return ls;
}

/* static */ ProgressSender& Client::progressSender()
{
    static ProgressSender ps(processManager());
    return ps;
}

const std::string Client::defaultUserAgent("Arras Native Client");

Client::Client()
    : Client(defaultUserAgent)
{
}

Client::Client(const std::string& aUserAgent)
    : mPort(0)
    , mPeer(nullptr)
    , mState(STATE_DISCONNECTED)
    , mConnectionError(false)
    , mUserAgent(aUserAgent)
    , mRun(false)
    , mSendAsync(false)
    , mIsLocal(false)
{
}

Client::~Client()
{
    disconnect();
    if (mIsLocal) {
	localSessions().abandonSession(api::UUID(mSessionId));
    }
}

void
Client::addComponent(Component* aComponent)
{
    mComponents.push_back(aComponent);
}

void
Client::removeComponent(Component* aComponent)
{
    mComponents.remove(aComponent);
}

void
Client::addExceptionCallback(const ExceptionCallback& callback)
{
    mExceptionCallbacks.push_back(callback);
}

void
Client::removeExceptionCallback(const ExceptionCallback& callback)
{
    auto del = callback.target<void(*)(const std::exception&)>();
    for (auto it = mExceptionCallbacks.begin();
         it != mExceptionCallbacks.end(); ++it) {
        auto f1 = (*it).target<void(*)(const std::exception&)>();
        if (!f1 || (f1 && del && *f1 == *del)) {
            it = mExceptionCallbacks.erase(it);
        }
    }
}

/* static */ const std::string
Client::getArrasUrl_static(const std::string& datacenter, 
                           const std::string& environment,
                           const std::string& userAgent)
{
    // debug override
    const char* overrideUrl = getenv(ENV_OVR_COORDINATOR_URL);
    if (overrideUrl) {
	ARRAS_WARN(log::Id("ClientOverrideActive") <<
		   "Client override: " << ENV_OVR_COORDINATOR_URL << " = " << overrideUrl);
	return std::string(overrideUrl) + "/sessions";
    }

    if (datacenter == "local")
	return LOCAL_SESSION_URL;

    // Query the dwa studio configuration service for the endpoint.
    return getResourceFromConfigurationService_static(COORDINATOR_CONFIG_ENDPOINT,
                                                      datacenter,
                                                      environment,
                                                      userAgent) + "/sessions";
}

const std::string
Client::requestArrasUrl(const std::string& datacenter, const std::string& environment)
{
	return getArrasUrl_static(datacenter,environment,mUserAgent);
}

bool
Client::sessionExists(const std::string& sessionId, const std::string& datacenter, const std::string& environment) const
{
	if (sessionId.empty()) {
	    throw ClientException("Unable to query sessions/sessionid: invalid(empty) sessionId",
				  ClientException::GENERAL_ERROR);
	}

	// Build the session query url:
	const std::string& sessionQueryUrl = getCoordinatorEndpoint(datacenter, environment) + 
	    SESSIONS_PATH + "/" + sessionId;

	HttpRequest req(sessionQueryUrl);
	const HttpResponse& resp = req.submit();

	if (resp.responseCode() == HTTP_OK) {
		return true;
	} else if (resp.responseCode() == HTTP_NOT_FOUND) {
		return false;
	} else if (resp.responseCode() == HTTP_SERVICE_UNAVAILABLE) {
		const std::string& msg = "session query service unavailable: " + sessionQueryUrl;
		throw ClientException(msg,ClientException::CONNECTION_ERROR);
	} else {
		const std::string& msg = "Unable to query sessions/sessionid. response code: " +
            std::to_string(resp.responseCode()) + " url: " + sessionQueryUrl;
		throw ClientException(msg,ClientException::CONNECTION_ERROR);
	}
}


/* static */ const std::string
Client::getResourceFromConfigurationService_static(const std::string& resourcePath,
                                                   const std::string& datacenter, 
                                                   const std::string& environment,
                                                   const std::string& userAgent)
{
	// Get dwa studio configuration service url from env
    const char* configSrvUrl = std::getenv(DWA_CONFIG_ENV_NAME);
    if (configSrvUrl == nullptr) {
        throw ClientException(std::string("Undefined environment variable: ") + DWA_CONFIG_ENV_NAME,
                              ClientException::CONNECTION_ERROR);
    }

    // Build the resource url
	std::string url(configSrvUrl);
	url += ARRAS_CONFIG_PATH;
	url += datacenter;
	url += "/";
	url += environment;
	url += resourcePath;

	// Make the service request
    HttpRequest req(url);
    req.setUserAgent(userAgent);
    const HttpResponse& resp = req.submit();

    if (resp.responseCode() == HTTP_OK) {
        std::string ret;
        if (resp.getResponseString(ret))
            return ret;
        throw ClientException("Coniguration service returned empty response",ClientException::CONNECTION_ERROR);
    } else if (resp.responseCode() == HTTP_SERVICE_UNAVAILABLE) {
    	const std::string& msg = "Configuration service unavailable: " + url;
        throw ClientException(msg,ClientException::CONNECTION_ERROR);
    } else {
    	const std::string& msg = "Unexpected response code from configuration service. The response code was : " +
    			std::to_string(resp.responseCode()) + ", the url was : " + url;
        throw ClientException(msg,ClientException::CONNECTION_ERROR);
    }
}

const std::string
Client::getResourceFromConfigurationService(const std::string& resourcePath,
                                            const std::string& datacenter, 
                                            const std::string& environment) const
{
    return getResourceFromConfigurationService_static(resourcePath,datacenter, 
                                                      environment,mUserAgent);
}

const std::string
Client::getCoordinatorEndpoint(const std::string& datacenter, const std::string& environment) const
{
	ARRAS_LOG_INFO("Client::getCoordinatorEndpoint '%s' '%s'", datacenter.c_str(), environment.c_str());

	// debug override
	const char* overrideUrl = getenv(ENV_OVR_COORDINATOR_URL);
	if (overrideUrl) {
	    ARRAS_WARN(log::Id("ClientOverrideActive") <<
		       "Client override: " << ENV_OVR_COORDINATOR_URL << " = " << overrideUrl);
	    return overrideUrl;
	}

	if (datacenter.empty() || environment.empty()) {
		// local server
		return DEFAULT_LOCAL_COORDINATOR_ENDPOINT;
	} else {
		// Query the dwa studio configuration service for the endpoint.
		return getResourceFromConfigurationService(COORDINATOR_CONFIG_ENDPOINT, datacenter, environment);
	}
}


// TODO: this structure is defined in RegistrationData.h on the service side, make it available in "common" somewhere?
struct RegistrationData {
    typedef uint64_t MagicType;
    static const MagicType MAGIC = 0x0104020309060201L;
    uint64_t mMagic;
    unsigned short mMessagingAPIVersionMajor;
    unsigned short mMessagingAPIVersionMinor;
    unsigned short mMessagingAPIVersionPatch;
    unsigned short mReserved; // for alignment

    // changes above this point may break protocol version checking


    api::UUID mSessionId;
    api::UUID mNodeId;
    api::UUID mExecId;
    int mType = 0; // our type is 0

    RegistrationData(unsigned short major, unsigned short minor, unsigned short patch) :
                     mMagic(MAGIC),
                     mMessagingAPIVersionMajor(major),
                     mMessagingAPIVersionMinor(minor),
                     mMessagingAPIVersionPatch(patch),
                     mReserved(0) {};
};

void
Client::connectTCP()
{
    if (getState() != STATE_CONNECTING) {
        throw ClientException("Cannot connect to node unless Session has been created",
                              ClientException::GENERAL_ERROR);
    }

    // store this in a unique_ptr so any exceptions will clean up properly
    std::unique_ptr<InetSocketPeer> inetSocketPeer(new InetSocketPeer(mHostIp, mPort));

    // register with the node
    RegistrationData regData(ARRAS_MESSAGING_API_VERSION_MAJOR,
                             ARRAS_MESSAGING_API_VERSION_MINOR,
                             ARRAS_MESSAGING_API_VERSION_PATCH);
    regData.mSessionId = api::UUID(mSessionId.c_str());
    inetSocketPeer->send_or_throw(&regData, sizeof(regData),"Client::connectTCP");

    // steal the object out of the unique ptr since it will now
    // persist in the Client object
    mPeer.reset(inetSocketPeer.release());

    postConnect();
}
  
void Client::disableMessageChunking()
{
    mChunkingConfig.enabled = false;
}

void Client::enableMessageChunking(size_t minChunkingSize,size_t chunkSize)
{
     mChunkingConfig.enabled = true;
     if (minChunkingSize) mChunkingConfig.minChunkingSize = minChunkingSize;
     if (chunkSize) mChunkingConfig.chunkSize = chunkSize;
}

void
Client::postConnect()
{
    ARRAS_LOG_TRACE("Connected, sent '%s'", mSessionId.c_str());

    // change state to "connected"
    setState(STATE_CONNECTED);
    mRun = true;
    mEngineReady = false;
    mPeerEndpoint = new PeerMessageEndpoint(*mPeer,true,"client entry");
    mMessageEndpoint = new ChunkingMessageEndpoint(*mPeerEndpoint,mChunkingConfig);
    if (mSendAsync) {
        mOutgoingQueue = new MessageQueue("outgoing");
        mSendThread = std::thread(&Client::sendProc,this);
    }

    // configure message recording. These values were setup earlier in createSession() by
    // calling setupMessageRecording()
    if (!mIncomingSaveDir.empty()) {
        mPeerEndpoint->reader().enableAutosave(mIncomingSaveDir);
    }
    if (!mOutgoingSaveDir.empty()) {
        mPeerEndpoint->writer().enableAutosave(mOutgoingSaveDir);
    }

    mThread = std::thread(&Client::threadProc, this);

    // send a "ready" message to node to announce being connected
    arras4::impl::ControlMessage* controlMessage = new arras4::impl::ControlMessage("ready","","");
    const api::MessageContent* cp = controlMessage;
    send(MessageContentConstPtr(cp));
}

void
Client::shutdownConnection()
{
    if (mPeer != nullptr) {
        try {
            // if the receive thread is running, end and join it
            mRun = false;
            if (mOutgoingQueue)
                mOutgoingQueue->shutdown();
            if (mMessageEndpoint)
                mMessageEndpoint->shutdown();
            mPeer->shutdown();
            delete mMessageEndpoint;
            delete mOutgoingQueue;
            mMessageEndpoint = nullptr;
            mOutgoingQueue = nullptr;
            delete mPeerEndpoint;
            mPeerEndpoint = nullptr;
        } catch (PeerException& /*e*/) {
            // its getting deleted in any case so nothing to do here
        }

        if (mThread.joinable()) mThread.join();
        if (mSendThread.joinable()) mSendThread.join();
        if (mIsLocal) shutdownLocal();
        mPeer.reset();
        mConnectionError = false;
    }
}

void
Client::sendShutdownMessage()
{
    // called when client requests a clean shutdown (Sdk::shutdownSession)

    if (mIsLocal) {
	shutdownLocal();
    } else {
	shutdownDistributed();
    }
}

void 
Client::shutdownLocal()
{
  try {
      localSessions().stopSession(api::UUID(mSessionId));
  } catch (std::exception& e) {
      throw ClientException("Failed to delete local session: " + std::string(e.what()));
  }
  setState(STATE_DISCONNECTING);
}

void
Client::shutdownDistributed()
{
    arras4::impl::ControlMessage* controlMessage = new arras4::impl::ControlMessage("disconnect","","");
    const api::MessageContent* cp = controlMessage;
    send(MessageContentConstPtr(cp));
    setState(STATE_DISCONNECTING);
}

void
Client::disconnect()
{
    setState(STATE_DISCONNECTING);
    shutdownConnection();
    setState(STATE_DISCONNECTED);
}

bool
Client::isConnected() const
{
    return getState() == STATE_CONNECTED;
}

bool
Client::isDisconnected() const
{
    return getState() == STATE_DISCONNECTED;
}

bool
Client::isEngineReady() const
{
    return mEngineReady;
}

bool
Client::isErrored() const
{
    return mConnectionError;
}

bool 
Client::waitForEngineReady(unsigned maxSeconds) const
{
    // debug override
    const char* overrideSecs = getenv(ENV_OVR_READY_WAIT_SECS);
    if (overrideSecs) {
	try {
	    unsigned long longSecs = std::stoul(overrideSecs);
	    ARRAS_WARN(log::Id("ClientOverrideActive") <<
		       "Client override: " << 
		       ENV_OVR_READY_WAIT_SECS << " = " << longSecs);
	    maxSeconds = static_cast<unsigned>(longSecs);
	} catch (std::exception&) {
	    ARRAS_ERROR(log::Id("ClientOverrideFailed") <<
			"Invalid client override ignored: " << 
			ENV_OVR_READY_WAIT_SECS << " = " << overrideSecs);
	}
    }

    while (!mEngineReady && !mConnectionError && 
           isConnected() && maxSeconds > 0) {
    	std::this_thread::sleep_for(std::chrono::seconds(1));
        maxSeconds--;
    }
    return mEngineReady;
}

bool
Client::waitForDisconnect(unsigned maxSeconds) const
{
    // debug override
    const char* overrideSecs = getenv(ENV_OVR_DISCONNECT_WAIT_SECS);
    if (overrideSecs) {
	try {
	    unsigned long longSecs = std::stoul(overrideSecs,nullptr,10);
	    ARRAS_WARN(log::Id("ClientOverrideActive") <<
		       "Client override: " << 
		       ENV_OVR_DISCONNECT_WAIT_SECS << " = " << longSecs);
	    maxSeconds = static_cast<unsigned>(longSecs);
	} catch (std::exception&) {
	    ARRAS_ERROR(log::Id("ClientOverrideFailed") <<
			"Invalid client override ignored: " << 
			ENV_OVR_DISCONNECT_WAIT_SECS << " = " << overrideSecs);
	}
    }

    while (!isDisconnected() && maxSeconds > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        maxSeconds--;
    }
    return isDisconnected();
}

void
Client::pause()
{
    // todo: not yet implemented for distributed sessions
    if (mIsLocal) localSessions().pauseSession(mSessionId);
}

void
Client::resume()
{
    // todo: not yet implemented for distributed session
    if (mIsLocal) localSessions().resumeSession(mSessionId);
}

int
Client::numSessionDefinitions() const
{
    return int(mSessionDefinitions.size());
}


const char*
Client::sessionKey(int aIdx)
{
    assert(aIdx >= 0 && size_t(aIdx) < mSessionDefinitions.size());
    if (aIdx >= 0 && size_t(aIdx) < mSessionDefinitions.size()) {
        return mSessionDefinitions[aIdx].c_str();
    }

    return 0;
}

void
Client::addHTTPBodyAttr(api::ObjectRef aBody, const std::string& aName, const std::string& aValue) const
{
    /* We are going to treat empty strings as null values */
    if (!aValue.empty()) {
        aBody[aName] = aValue;
    } else {
        aBody[aName] = api::Object();
    }
}

void
Client::addHTTPBodyAttr(api::ObjectRef aBody, const std::string& aName, api::ObjectConstRef aValue) const
{
    aBody[aName] = aValue;
}


void 
Client::makeCreateRequest(const SessionDefinition& aDefinition, 
                          const SessionOptions& aSessionOptions,
                          const PlatformInfo& info,
                          const std::string& username,
                          api::ObjectRef reqObj)
{
    setState(STATE_CONNECTING);
    
    reqObj["session"] = "empty";
    reqObj["node_name"] = info.mNodeName;
    reqObj["os_name"] = info.mOSName;
    reqObj["os_version"] = info.mOSVersion;
    reqObj["os_distribution"] = info.mOSDistribution;
    reqObj["brief_version"] = info.mBriefVersion;
    reqObj["brief_distribution"] = info.mBriefDistribution;

    reqObj["pid"] = getpid();

    reqObj["username"] = username;

    addHTTPBodyAttr(reqObj, "production", aSessionOptions.getProduction());
    addHTTPBodyAttr(reqObj, "sequence", aSessionOptions.getSequence());
    addHTTPBodyAttr(reqObj, "shot", aSessionOptions.getShot());

    addHTTPBodyAttr(reqObj, "assetGroup", aSessionOptions.getAssetGroup());
    addHTTPBodyAttr(reqObj, "asset", aSessionOptions.getAsset());

    addHTTPBodyAttr(reqObj, "department", aSessionOptions.getDepartment());
    addHTTPBodyAttr(reqObj, "team", aSessionOptions.getTeam());

    addHTTPBodyAttr(reqObj, "sessionDef", aDefinition.getObject());
    addHTTPBodyAttr(reqObj, "metadata",aSessionOptions.getMetadata());
}

void 
Client::connectSession(api::ObjectConstRef responseObject)
{
    setState(STATE_CONNECTING);

    std::string error;
    if (responseObject["sessionId"].isString()) {
        mSessionId = responseObject["sessionId"].asString();
    } else {
        error = "Server returned invalid sessionId";
    }

    // response contains our TCP hostname, TCP port and session UUID
    if (error.empty()) {
        if (responseObject["hostname"].isString())
            mHostname = responseObject["hostname"].asString();
        else
            error = "Server returned invalid host name";
    }

    if (error.empty()) {
        if (responseObject["ip"].isString())
            mHostIp = responseObject["ip"].asString();
        else
            error = "Server returned invalid ip";
    }

    if (error.empty()) {
        if (responseObject["port"].isIntegral())
            mPort = (unsigned short)(responseObject["port"].asInt());
        else
	        error = "Server returned invalid port number";
    }
    
    if (error.empty()) {
	progress("Connecting");
       
	ARRAS_LOG_INFO("Connecting via host:port (%s:%d) (hostname %s)", mHostIp.c_str(), 
		       mPort, mHostname.c_str());
	try {
	    connectTCP();
	    setState(STATE_CONNECTED);
	} catch (std::exception& e) {
	    error = std::string("Connection failed: ") + e.what();
	}
    }

    if (error.empty()) {
	ARRAS_LOG_INFO("Connected");
	progress("Connected");
	progressInfo("id",mSessionId);
    } else {
	progress("Connection failed","failed");
	progressInfo("errors",error);
	throw ClientException(error, ClientException::GENERAL_ERROR);
    }
}


std::string
Client::createSession(const SessionDefinition& aDefinition, 
                      const std::string& aUrl,
                      const SessionOptions& aSessionOptions)
{
    // check definitions is complete
    if (!aDefinition.checkNamedContexts()) {
	throw ClientException("Session contains an unresolved context name");
    }

    // debug overrides for log and trace level are applied every time
    // you connect, in an attempt to bypass any values set by the client
    // code
    applyLoggingOverrides();

    disconnect(); // Disconnect first in case we had a connection error

    mArrasUrl = aUrl;

    // client override
    bool forceLocal = false;
    api::ObjectConstRef comps = aDefinition.getObject()["computations"];
    if (comps.isObject() && (comps.size() == 2)) {
	const char* forceLocalVal = getenv(ENV_OVR_FORCE_LOCAL_MODE);
	if (forceLocalVal) {
	    std::string forceLocalStr(forceLocalVal);
	    forceLocal = (forceLocalStr == "true" || forceLocalStr == "TRUE" ||
			  forceLocalStr == "yes" || forceLocalStr == "YES" ||
			  forceLocalStr == "1");
	}
    }

    bool local = forceLocal || (aUrl == LOCAL_SESSION_URL);

    // initialize progress GUI
    // - set to aSessionOptions' id if it's not empty, else 
    //   generate a progress id (can't use Arras session id, since
    //   we don't know it yet)
    mProgressId = aSessionOptions.getId().empty() ? 
        std::string("progress@") + api::UUID::generate().toString() :
        aSessionOptions.getId();
    ARRAS_DEBUG("setting mProgressId: " << mProgressId);

    api::Object progMsg;
    progMsg["id"] = mProgressId;
    progMsg["title"] = aSessionOptions.getTitle();
    progMsg["start"] = api::ArrasTime::now().dateTimeStr();
    progMsg["type"] = local ? "Local" : "Pool";
    progMsg["stage"] = local ? "Creating" : "Requesting resources";
    progMsg["status"] = "pending";
    progMsg["progress"] = "";
    progressSender().progress(progMsg);

    if (local) {
	return createLocal(aDefinition,aSessionOptions);
    } else {
	return createDistributed(aDefinition,aUrl,aSessionOptions);
    }
}

void 
Client::localTermination(bool /*expected*/,
			 api::ObjectConstRef data)
{
    std::string dataStr = api::objectToString(data);
    ARRAS_DEBUG("Local computation terminated: " << dataStr);
    impl::SessionStatusMessage* statusMessage = new impl::SessionStatusMessage(dataStr);
    Envelope env(statusMessage);
    Message msg = env.makeMessage();
    for (auto it = mComponents.begin(); it != mComponents.end(); ++it) {
	if (*it) (*it)->onStatusMessage(msg);
    }
    if (mState == STATE_DISCONNECTING) {
	mState = STATE_DISCONNECTED;
    }
}
std::string 
Client::createLocal(const SessionDefinition& aDefinition,
		    const SessionOptions& aSessionOptions)
{
    
    std::shared_ptr<LocalSession> sp;
    std::string error("Failed to create local session");

    try {
        std::string sessionId = api::UUID::generate().toString();

        // convert sessionOptions to JSON
        api::Object sessionOptions;
        aSessionOptions.appendToObject(sessionOptions);

        // notify coordinator of the session
        new SessionNotification(aDefinition.getObject(),sessionOptions,
                                sessionId, mUserAgent);

	sp = localSessions().createSession(aDefinition.getObject(), sessionId,
					  std::bind(&Client::localTermination,this,
						    std::placeholders::_1, std::placeholders::_2));
    } catch (std::exception& e) {
	error += std::string(" :") + e.what();
    }
    if (!sp) {
	progress("Creation failed","failed");
	progressInfo("errors",error);
	throw ClientException(error);
    }

    mSessionId = sp->address().session.toString();
    mPeer = sp->peer();
    postConnect(); 

    progress("Created");
    progressInfo("id",mSessionId);

    mIsLocal = true;

    // writes a log line to the user's home dir (this is
    // unconnected with athena logging, and used to make it easier to
    // find the session id based on user name and approx time)
    logLocal(aDefinition,aSessionOptions);
    
    // send 'go' message to computation
    arras4::impl::ControlMessage* controlMessage = new arras4::impl::ControlMessage("go","","");
    const api::MessageContent* cp = controlMessage;
    send(MessageContentConstPtr(cp));

    mEngineReady = true; 

    progress("Running");
 
    ARRAS_ATHENA_TRACE(0,log::Session(mSessionId) <<
                           "{trace:session} clientConnect " <<
		       mSessionId << " local " << getClientVersion());

    return mSessionId;
}

void 
Client::logLocal(const SessionDefinition& aDefinition,
		 const SessionOptions& aSessionOptions)
{
    // writes a log line to the user's home dir
    const char* home = std::getenv("HOME");
    if (home == nullptr) {
	ARRAS_WARN(log::Id("LocalLogFailed") <<
		   "Cannot log local session : $HOME is not defined");
	return;
    }
    std::string username("unknown_user");
    const char* logname = std::getenv("LOGNAME");
    if (logname != nullptr) username = logname;
    std::string sessname("unnamed_session");
    if (aDefinition.getObject()["name"].isString()) {
	sessname = aDefinition.getObject()["name"].asString();
    }

    try {
	boost::filesystem::path logfile(home);
	logfile /= LOCAL_LOG_DIR;
	boost::filesystem::create_directory(logfile);
	logfile /= LOCAL_LOG_NAME;
	std::ofstream stream(logfile.c_str(),std::ios_base::app);
	stream << api::ArrasTime::now().dateTimeStr() << " " << username << " " << sessname << " " <<
	    mSessionId << " Sq " << aSessionOptions.getSequence() << " S " << aSessionOptions.getShot() <<
	    " AssGrp " << aSessionOptions.getAssetGroup() << " Ass " << aSessionOptions.getAsset() << std::endl;
	stream.close();
    } catch (std::exception &e) {
	ARRAS_WARN(log::Id("LocalLogFailed") <<
		   "Cannot log local session : " << e.what());
    } catch (...) {
	ARRAS_WARN(log::Id("LocalLogFailed") <<
		   "Cannot log local session : Unknown exception");
    }
}
	
	    

std::string 
Client::createDistributed(const SessionDefinition& aDefinition, 
			  const std::string& aUrl,
			  const SessionOptions& aSessionOptions)
{
    PlatformInfo info;
    getPlatformInfo(info);
      
    const char* logname = std::getenv("LOGNAME");
    if (logname == nullptr) {
	progress("Request error");
	progressInfo("errors","Unable to determine the current user's login name");
        throw ClientException("Unable to determine the current user's login name",
                              ClientException::GENERAL_ERROR);
    }

    api::Object reqObj;
    makeCreateRequest(aDefinition, 
                      aSessionOptions,
                      info, logname, reqObj);

    HttpRequest req(aUrl, POST);
    req.setUserAgent(mUserAgent);

    try {
	std::string httpMsgBody = api::objectToString(reqObj);
	req.setContentType(arras4::network::APPLICATION_JSON);

	// Make the Session Request
	ARRAS_LOG_DEBUG_STR("POST " << aUrl);
	ARRAS_LOG_DEBUG_STR("POST Body: " << httpMsgBody);
    
	const HttpResponse& resp = req.submit(httpMsgBody);

	// there are 4 ways an error could be indicated:
	//  - no response body
	//  - response body not JSON
	//  - response body is JSON containing an "error" member
	//  - response code is not HTTP_OK

	std::string responseString; 
	bool emptyResponse = false;
	std::string responseMessage;
	api::Object responseObject;

	if (resp.getResponseString(responseString)) {
	    responseMessage = responseString;
	    ARRAS_LOG_DEBUG_STR("Http Response (" << resp.responseCode() << "): " << responseMessage);
	    // got a string : is it JSON?
	    try {
		api::stringToObject(responseString, responseObject);

		// its an object, does it have a message?
		if (responseObject.isMember("message")) {
		    if (responseObject["message"].isString()) {
			responseMessage = responseObject["message"].asString();
		    }
		}
	    } catch (api::ObjectFormatError&) {
		// not an object : assume its an error message string
	    }
	} else {
	    // no response body
	    emptyResponse = true;
	}

	if (!emptyResponse && resp.responseCode() == HTTP_OK) {

	    setupMessageRecording(aDefinition);
	    connectSession(responseObject); 
	
	} else if (resp.responseCode() == HTTP_SERVICE_UNAVAILABLE) {
	    // don't care about error message : status code is enough to tell us
	    // what happened...
	    std::string err("Insufficient resources available to fill this request");
	    if (!responseMessage.empty())  {
		err += ": " + responseMessage;
	    }
	    
	    progress("No resources","failed");
	    progressInfo("errors",err);

	    throw ClientException(err, ClientException::NO_AVAILABLE_RESOURCES_ERROR);
	} else {
	    // unknown status code must be an error
	    std::string err = std::string("Server responded with error code ") + std::to_string(resp.responseCode());
	    if (!responseMessage.empty())
		err += ", message: " + responseMessage;
	    
	    progress("Request failed","failed");
	    progressInfo("errors",err);

	    throw ClientException(err,ClientException::GENERAL_ERROR);
	}
    } catch (std::exception& e) {
	progress("Request failed","failed");
	std::string err = std::string("Failed to connect to Coordinator: ")+e.what();
	progressInfo("errors",err);
	throw ClientException(err,
			      ClientException::GENERAL_ERROR);
    }
    mIsLocal = false;

    ARRAS_ATHENA_TRACE(0,log::Session(mSessionId) <<
                           "{trace:session} clientConnect " <<
		       mSessionId << " remote " << getClientVersion());
    return mSessionId;
}

void 
Client::setupMessageRecording(const SessionDefinition& aDefinition)
{
    // message recording options for the client are similar to
    // those for computations, but stored under "(client)"
    ObjectConstRef clientParams(aDefinition["(client)"]);
    if (!clientParams.isObject())
	return;

    // mIncomingSaveDir and mOutgoingSaveDir are used during the
    // postConnect function to setup autosave on the message reader/writer
    if (clientParams["saveIncomingTo"].isString()) {
        mIncomingSaveDir = clientParams["saveIncomingTo"].asString();
    } else {
        mIncomingSaveDir = std::string();
    }
    if (clientParams["saveOutgoingTo"].isString()) {
        mOutgoingSaveDir = clientParams["saveOutgoingTo"].asString();
    } else {
        mOutgoingSaveDir = std::string();
    }

    // if "saveDefinitionTo" is set, session definition is saved immediately,
    // but with recording options cleared (since you probably don't want
    // playback to override the recording)
    if (clientParams["saveDefinitionTo"].isString()) {
        std::string savePath = clientParams["saveDefinitionTo"].asString();
        SessionDefinition defCopy(aDefinition.getObject());
        // removeMember() does nothing if member doesn't exist..
        defCopy["(client)"].removeMember("saveIncomingTo");
        defCopy["(client)"].removeMember("saveOutgoingTo");
        defCopy["(client)"].removeMember("saveDefinitionTo");
        defCopy.saveToFile(savePath);
    }
}
        
size_t
Client::send(const MessageContentConstPtr& content,
                 ObjectConstRef options)
{
    if (mSendAsync)
        return sendAsync(content,options);
    else
        return sendSync(content,options);
}

size_t
Client::sendSync(const MessageContentConstPtr& content,
                 ObjectConstRef options)
{
    if (mConnectionError) disconnect();
    if (getState() != STATE_CONNECTED) {
        throw ClientException("Can't send a message if client is disconnected",
                              ClientException::GENERAL_ERROR);
    }

    if (mMessageEndpoint) {
        Envelope env(content,options);     
        Address addr;
        addr.session = UUID(mSessionId.c_str());
        env.metadata()->from() = addr;
        ARRAS_ATHENA_TRACE(2,log::Session(mSessionId) <<
                           "{trace:message} post " <<
                           env.metadata()->instanceId().toString() <<
                           " (client) " <<
                           env.metadata()->sourceId().toString() << " " <<
                           env.metadata()->routingName() << " " <<
                           content->classId().toString());

        // set trace flag if logger trace level >= 3
        // this will cause additional tracing as message is transported
        if (log::Logger::instance().traceThreshold() >= 3) {
            env.metadata()->trace() = true;
        }
        try {
            mMessageEndpoint->putEnvelope(env);
        } catch (std::exception& e) {
            throw ClientException(e.what(),ClientException::SEND_ERROR);
        }
    }

    return 0;
}

size_t
Client::sendAsync(const MessageContentConstPtr& content,
                  ObjectConstRef options)
{
    if (mConnectionError) disconnect();
    if (getState() != STATE_CONNECTED) {
        throw ClientException("Can't send a message if client is disconnected",
                              ClientException::GENERAL_ERROR);
    }

    Envelope env(content,options);
    Address addr;
    addr.session = UUID(mSessionId.c_str());
    env.metadata()->from() = addr;
    ARRAS_ATHENA_TRACE(2,log::Session(mSessionId) <<
                       "{trace:message} post " <<
                       env.metadata()->instanceId().toString() <<
                       " (client) " <<
                       env.metadata()->sourceId().toString() << " " <<
                       env.metadata()->routingName() << " " <<
                       content->classId().toString());

    // set trace flag if logger trace level >= 3
    // this will cause additional tracing as message is transported
    if (log::Logger::instance().traceThreshold() >= 3) {
        env.metadata()->trace() = true;
    }

    try {
        mOutgoingQueue->push(env);
    } catch (ShutdownException&) {
        throw ClientException("Can't send a message : send queue is shut down",
                            ClientException::GENERAL_ERROR);
    }  catch (std::exception& e) {
        throw ClientException(e.what(),ClientException::SEND_ERROR);
    }
    return 0;
}

void
Client::sendProc()
{
    arras4::log::Logger::instance().setThreadName("message send");
    while (mRun) {
        try {
                Envelope envelope;
                mOutgoingQueue->pop(envelope);
                if (mMessageEndpoint) {
                    mMessageEndpoint->putEnvelope(envelope);
                }
        } catch (ShutdownException&) {
            // queue has been unblocked to give us a chance to exit
        } catch (network::PeerDisconnectException&) {
            // disconnected, ignore
        } catch (std::exception& e) {
            // notify client of send exception
            for (auto it : mExceptionCallbacks) {
                it(e);
            }
        } catch (...) {
            ARRAS_ERROR(log::Id("clientUnhandled") <<
                        log::Session(mSessionId) <<
                        "Unhandled exception in send thread");
        }
    }
}

void
Client::deliverMessages()
{
    if (mMessageEndpoint) {
        Envelope env = mMessageEndpoint->getEnvelope();
        Message msg = env.makeMessage();

        ARRAS_ATHENA_TRACE(2,log::Session(mSessionId) <<
                           "{trace:message} dispatch " <<
                           env.metadata()->instanceId().toString() <<
                           " (client) " <<
                           env.metadata()->routingName());

        if (msg.classId() == EngineReadyMessage::ID) {
            mEngineReady = true;
            for (auto it = mComponents.begin(); it != mComponents.end(); ++it) {
                if (*it) (*it)->onEngineReady();
            }
        } else if (msg.classId() == SessionStatusMessage::ID) {
            for (auto it = mComponents.begin(); it != mComponents.end(); ++it) {
                if (*it) (*it)->onStatusMessage(msg);
            }
        } else if (msg.classId() != ExecutorHeartbeat::ID) {
	    // ExecutorHeartbeat 
            for (auto it = mComponents.begin(); it != mComponents.end(); ++it) {
                if (*it) (*it)->onMessage(msg);
            }
        }
        ARRAS_ATHENA_TRACE(2,log::Session(mSessionId) <<
                           "{trace:message} handled " <<
                           env.metadata()->instanceId().toString() <<
                           " (client) " <<
                           env.metadata()->routingName() << " 0");
    }
}

void
Client::threadProc()
{
    arras4::log::Logger::instance().setThreadName("message delivery");
    while (mRun) {
        try {
            deliverMessages();
        } catch (const PeerException& e) {
            const auto code = e.code();
            if (code == PeerException::CONNECTION_CLOSED) {
                // if we were expecting a disconnection, don't report an error

                if (getState() == STATE_DISCONNECTING) {

                    // register socket disconnection with Coordinator
                    // (help to identify network issues : ARRAS-3693)
                    registerDisconnect(true,e.what());

                    // don't disconnect yet in local mode,
                    // -- we have to wait until the computation terminates
                    // otherwise the session status won't be available.
                    // DISCONNECTED state will be set in 'localTermination()'
                    if (!mIsLocal) {
                        mState = STATE_DISCONNECTED;
                    }
                    break;
                }
            }

            ARRAS_ERROR(log::Id("clientSocketError") <<
                        log::Session(mSessionId) <<
                        e.what());

            // register socket disconnection with Coordinator
            // (help to identify network issues : ARRAS-3693)
            registerDisconnect(false,e.what());

            for (auto it : mExceptionCallbacks) {
                it(e);
            }

            mConnectionError = true;
            break;
        } catch (const ShutdownException&) {
            ARRAS_DEBUG("MessageEndpoint was shut down");
        } catch (...) {
            ARRAS_ERROR(log::Id("clientUnhandled") <<
			log::Session(mSessionId) <<
			"Unhandled exception in delivery thread");
            throw;
        } 
        
    }
}
 
void
Client::registerDisconnect(bool expected,
			   const std::string& message)
{
    if (mIsLocal)
	return;

    // POST a client disconnect event to Coordinator
    std::string eventUrl = mArrasUrl + "/" + mSessionId + "/event";
    api::Object body;
    body["type"] =  expected ? "clientExpectedDisconnect" : "clientUnexpectedDisconnect";
    body["message"] = message;

    std::string err;
    try {
	HttpRequest req(eventUrl, POST);
	req.setUserAgent(mUserAgent);

	std::string httpMsgBody = api::objectToString(body);
	req.setContentType(arras4::network::APPLICATION_JSON);

	const HttpResponse& resp = req.submit(httpMsgBody);
	if (resp.responseCode() < HTTP_OK ||
	    resp.responseCode() >= HTTP_BAD_REQUEST) {
	    ARRAS_LOG_WARN("Failed to notify Coordinator of client disconnect event: status %d",resp.responseCode());
	}
    } catch (std::exception& e) {
	ARRAS_LOG_WARN("Failed to notify Coordinator of client disconnect event: %s",e.what());
    }
}

impl::PeerMessageEndpoint*
Client::endpoint()
{
    return mPeerEndpoint;
}
  
void 
Client::progress(const std::string& stage, unsigned percent)
{
    api::Object msg;
    msg["id"] = mProgressId;
    msg["stage"] = stage;
    msg["progress.percent"] = percent;
    progressSender().progress(msg);
}
    
void 
Client::progress(const std::string& stage, 
		 const std::string& status,
		 const std::string& text)
{
    api::Object msg;
    msg["id"] = mProgressId;
    msg["stage"] = stage;
    msg["status"] = status;
    msg["progress"] = text;
    progressSender().progress(msg);
}

void Client::progressInfo(const std::string& category,
			  api::ObjectConstRef value)
{
    api::Object msg;
    msg["id"] = mProgressId;
    msg["addinfo"]["category"] = category;
    if (value.isString()) {
        msg["addinfo"]["text"] = value.asString();
    } else {
        msg["addinfo"]["value"] = value;
    }
    progressSender().progress(msg);
}

}
} 
 

