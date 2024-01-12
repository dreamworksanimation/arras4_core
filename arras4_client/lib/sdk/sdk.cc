// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "sdk_impl.h"

#include "arras_static_check.h"

#include <arras4_athena/AthenaLogger.h>
#include <arras4_log/Logger.h>
#include <core_messages/SessionStatusMessage.h>
#include <core_messages/ControlMessage.h>

#include <client/api/Client.h>
#include <client/api/RezResolve.h>
#include <client/api/Component.h>

#include <http/HttpException.h>
#include <message_api/Message.h>
#include <network/PeerException.h>

#include <cstring>
#include <cassert>

#include <mutex>

namespace arras4 {
    namespace sdk {
        namespace internal {


bool Impl::mCreatedLogger = false;
std::mutex Impl::mLoggerMutex;

bool
Impl::configAthenaLogger(const std::string& athenaEnv,
                         bool useColor,
                         const std::string& syslogHost,
                         unsigned short syslogPort)
{
    std::lock_guard<std::mutex> lock(mLoggerMutex);
    if (!mCreatedLogger) {
        log::AthenaLogger& logger = log::AthenaLogger::createDefault("client", useColor, athenaEnv, syslogHost, syslogPort);
        logger.setProcessName("client");
        ARRAS_THREADSAFE_STATIC_WRITE( mCreatedLogger = true; )
        return true;
    } else {
        return false;
    }
}


void
Impl::ComponentWrapper::onMessage(const api::Message& aMsg)
{
    
    ARRAS_LOG_DEBUG("Message received: %s", aMsg.describe().c_str());
    if (mImpl.mHandler) {
        mImpl.mHandler(aMsg);
    }
}

void
Impl::ComponentWrapper::onEngineReady()
{
    
    ARRAS_LOG_DEBUG("Engine ready received");
    if (mImpl.mEngineReadyCallback) {
        mImpl.mEngineReadyCallback();
    }
}

void
Impl::ComponentWrapper::onStatusMessage(const api::Message& aMsg)
{
    arras4::impl::SessionStatusMessage::ConstPtr statusMessage =
        aMsg.contentAs<arras4::impl::SessionStatusMessage>();
    if (statusMessage) {
        const std::string& statusString = statusMessage->getValue();
        if (mImpl.mStatusHandler) {
            mImpl.mStatusHandler(statusString);
        } else {
            ARRAS_LOG_INFO("Received status message: %s", statusString.c_str());
        }
    }
}

Impl::Impl()
    : mClient(new Client())
    , mHandler(nullptr)
    , mExceptionCallback(nullptr)
    , mEngineReadyCallback(nullptr)
{
    // make sure that the athena logger is initialized
    configAthenaLogger();

    mClient->addExceptionCallback(std::bind(&Impl::ExceptionCallbackWrapper, this, std::placeholders::_1));
    mComponent.reset(new ComponentWrapper(*this));
}

Impl::~Impl()
{
    disconnect();          // required so that the component which we're about to delete doesn't receive
                           // any more messages
    mComponent.reset();    // Now, we can delete these objects.
    mClient->removeExceptionCallback(std::bind(&Impl::ExceptionCallbackWrapper, this, std::placeholders::_1));
    mClient.reset();
}

void
Impl::setAsyncSend(bool flag)
{
    mClient->setAsyncSend(flag);
}

const std::string
Impl::requestArrasUrl(const std::string& datacenter, const std::string& environment)
{
    try {
        return mClient->requestArrasUrl(datacenter, environment);
    } catch (const network::HttpException& e) {
        throw SDKException(e.what(), SDKException::CONNECTION_ERROR);
    } 
}

const std::string& 
Impl::sessionId() const
{
    return mClient->sessionId();
}

bool
Impl::sessionExists(const std::string& sessionId, const std::string& datacenter, const std::string& environment) const
{
	try {
		return mClient->sessionExists(sessionId, datacenter, environment);
	} catch (const network::HttpException& e) {
        throw SDKException(e.what(), SDKException::CONNECTION_ERROR);
    }
}

std::string
Impl::createSession(const client::SessionDefinition& sessionDef, const std::string& url,
                    const client::SessionOptions& sessionOptions)
{
    try { 
        std::string sessionId = mClient->createSession(sessionDef, url, sessionOptions);
        return sessionId;
    } catch (std::exception& e) {
        throw SDKException(e.what(), SDKException::CONNECTION_ERROR);
    }
}

bool
Impl::isConnected() const
{
    return mClient->isConnected();
}

bool
Impl::isDisconnected() const
{
    return mClient->isDisconnected();
}

bool 
Impl::isEngineReady() const
{
    return mClient->isEngineReady();
}

bool 
Impl::isErrored() const
{
    return mClient->isErrored();
}

bool 
Impl::waitForEngineReady(unsigned maxSeconds) const
{
    return mClient->waitForEngineReady(maxSeconds);
}

bool
Impl::waitForDisconnect(unsigned maxSeconds) const
{
    return mClient->waitForDisconnect(maxSeconds);
}

void
Impl::shutdownSession()
{
    if (mClient->isConnected())
	mClient->sendShutdownMessage();
}

void
Impl::disconnect()
{
    mClient->disconnect();
}

void
Impl::pause()
{
    mClient->pause();
}

void
Impl::resume()
{
    mClient->resume();
}

void
Impl::sendMessage(const api::MessageContentConstPtr& msgPtr,
                  api::ObjectConstRef options) const
{
    mClient->send(msgPtr,options);
}

void
Impl::setMessageHandler(SDK::MessageHandler handler)
{
    mHandler = handler;
}

void
Impl::setStatusHandler(SDK::StatusHandler handler)
{
    mStatusHandler = handler;
}

void
Impl::setExceptionCallback(SDK::ExceptionCallback handler)
{
    mExceptionCallback = handler;
}

void
Impl::setEngineReadyCallback(SDK::EngineReadyCallback handler)
{
    mEngineReadyCallback = handler;
}

void
Impl::ExceptionCallbackWrapper(const std::exception& error)
{
    if (mExceptionCallback) {
        mExceptionCallback(error);
    }
}

void 
Impl::disableMessageChunking()
{
    mClient->disableMessageChunking();
}

void 
Impl::enableMessageChunking(size_t minChunkingSize, size_t chunkSize)
{
    mClient->enableMessageChunking(minChunkingSize,chunkSize);
}

void 
Impl::makeCreateRequest(const client::SessionDefinition& aDefinition, 
                        const client::SessionOptions& aSessionOptions,
                        const impl::PlatformInfo& info,
                        const std::string& username,
                        api::ObjectRef reqObj)
{
    mClient->makeCreateRequest(aDefinition,aSessionOptions,info,username,reqObj);
}

void
Impl::connectSession(api::ObjectConstRef responseObject)
{
    mClient->connectSession(responseObject);
}

void 
Impl::setupMessageRecording(const client::SessionDefinition& aDefinition)
{
    mClient->setupMessageRecording(aDefinition);
} 

impl::PeerMessageEndpoint*
Impl::endpoint()
{
    return mClient->endpoint();
}

void 
Impl::progress(const std::string& stage, unsigned percent)
{
    mClient->progress(stage,percent);
}
    
void 
Impl::progress(const std::string& stage, 
	       const std::string& status,
	       const std::string& text)
{
    mClient->progress(stage,status,text);
}

void
Impl::progressInfo(const std::string& category,
           api::ObjectConstRef value)
{
    mClient->progressInfo(category,value);
}

} // end namespace internal

/* ---------------------------------------------------------------------- */

SDK::SDK()
    : mImpl(new internal::Impl)
{
}

SDK::~SDK()
{
}

bool
SDK::configAthenaLogger(const std::string& athenaEnv,
                        bool useColor,
                        const std::string& syslogHost,
                        unsigned short syslogPort)
{

    // this messes with abstraction of Impl just being a pointer to an arbitraty implementation
    // but since it's static it leaves the size and layout of Impl abstracted so this doesn't
    // ruin anything important
    return internal::Impl::configAthenaLogger(athenaEnv, useColor, syslogHost, syslogPort);
}

void
SDK::setAsyncSend()
{
    mImpl->setAsyncSend(true);
}

void
SDK::setSyncSend()
{
    mImpl->setAsyncSend(false);
}

void
SDK::sendMessage(const api::MessageContentConstPtr& msgPtr,
                 api::ObjectConstRef options) const
{
    try {
        mImpl->sendMessage(msgPtr,options);
    } catch (const network::PeerException& e) {
        throw SDKException(e.what(), SDKException::SEND_ERROR);
    }
}

const std::string
SDK::requestArrasUrl(const std::string& datacenter, const std::string& environment)
{
    return mImpl->requestArrasUrl(datacenter, environment);
}

std::string
SDK::createSession(const client::SessionDefinition& sessionDef, 
                   const std::string& url,
                   const client::SessionOptions& sessionOptions)
{
    return mImpl->createSession(sessionDef, url, sessionOptions);
}

const std::string& 
SDK::sessionId() const
{
    return mImpl->sessionId();
}

bool
SDK::isConnected() const
{
    return mImpl->isConnected();
}
bool
SDK::isDisconnected() const
{
    return mImpl->isDisconnected();
}

bool
SDK::isEngineReady() const
{
    return mImpl->isEngineReady();
}

bool
SDK::isErrored() const
{
    return mImpl->isErrored();
}

bool
SDK::waitForEngineReady(unsigned seconds) const
{
    return mImpl->waitForEngineReady(seconds);
}

bool
SDK::waitForDisconnect(unsigned seconds) const
{
    return mImpl->waitForDisconnect(seconds);
}

void
SDK::shutdownSession()
{
    mImpl->shutdownSession();
}

void
SDK::disconnect()
{
    mImpl->disconnect();
}

void
SDK::pause()
{
    mImpl->pause();
}

void
SDK::resume()
{
    mImpl->resume();
}

void
SDK::setMessageHandler(MessageHandler messageHandler)
{
    mImpl->setMessageHandler(messageHandler);
}

void
SDK::setStatusHandler(StatusHandler messageHandler)
{
    mImpl->setStatusHandler(messageHandler);
}

void
SDK::setExceptionCallback(ExceptionCallback ExceptionCallback)
{
    mImpl->setExceptionCallback(ExceptionCallback);
}
void
SDK::setEngineReadyCallback(EngineReadyCallback callback)
{
    mImpl->setEngineReadyCallback(callback);
}

void 
SDK::disableMessageChunking()
{
    mImpl->disableMessageChunking();
}

void 
SDK::enableMessageChunking(size_t minChunkingSize, size_t chunkSize)
{
    mImpl->enableMessageChunking(minChunkingSize,chunkSize);
}

bool
SDK::sessionExists(const std::string& sessionId, const std::string& datacenter, const std::string& environment) const
{
	return mImpl->sessionExists(sessionId, datacenter, environment);
}

impl::PeerMessageEndpoint*
SDK::endpoint()
{
    return mImpl->endpoint();
}

/*static*/ std::string 
SDK::resolveRez(api::ObjectRef rezSettings,
		   std::string& err)
{
    return client::rezResolve(client::Client::processManager(),
			      rezSettings, err);
}

/*static*/ bool 
SDK::resolveRez(client::SessionDefinition& def,
		std::string& err)
{
    return client::rezResolve(client::Client::processManager(),
			      def, err);
}
    
void 
SDK::progress(const std::string& stage, unsigned percent)
{
    mImpl->progress(stage,percent);
}
    
void 
SDK::progress(const std::string& stage, 
	      const std::string& status,
	      const std::string& text)
{
    mImpl->progress(stage,status,text);
}

void
SDK::progressInfo(const std::string& category,
          api::ObjectConstRef value)
{
    mImpl->progressInfo(category,value);
}

/* static */ void
SDK::setProgressAutoExecCmd(const std::string& cmd) 
{
    client::Client::progressSender().setAutoExecCmd(cmd);
}

/* static */ void
SDK::setProgressChannel(const std::string& channel) 
{
    client::Client::progressSender().setChannel(channel);
}


} // end namespace sdk
} // end namespace arras

