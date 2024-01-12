// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MultiSession.h"
#include "sdk_impl.h"

#include <message_api/Message.h>
#include <message_api/Object.h>
#include <shared_impl/Platform.h>
#include <http/http_types.h>
#include <http/HttpResponse.h>
#include <http/HttpRequest.h>
#include <http/HttpException.h>
#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <cstdlib>
#include <chrono>
#include <functional> //std::bind & std::placeholders
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>

#if defined(JSONCPP_VERSION_MAJOR)
#define memberName name
#endif

using namespace arras4::api;
using namespace arras4::network;
using namespace arras4::impl;

namespace {
    const char *USER_AGENT = "Arras Native Client";
}

namespace arras4 {
    namespace sdk {

namespace internal {

class MultiImpl
{
public:
    MultiImpl();
    ~MultiImpl();
 
    SDK& addSession(const std::string& key, SessionHandler::Ptr handler); 
    SDK& getSession(const std::string& key); 
    MultiSession::SessionMap& getSessions();
    void removeSession(const std::string& key);
    void clear();
    void createAll(const std::string& arrasUrl);
    void sendAll(const api::MessageContentConstPtr& ptr,
                 api::ObjectConstRef options) const;
    bool allConnected() const;
    bool allDisconnected() const;
    bool allReady() const;
    bool waitForAllReady(unsigned maxSeconds) const;
    bool waitForAllDisconnected(unsigned maxSeconds) const;
    void disconnectAll();
    void shutdownAll();

private:
    void processCreateResponse(const HttpResponse& resp);

    MultiSession::SessionMap mMap;

};

MultiImpl::MultiImpl() 
{
}

MultiImpl::~MultiImpl() 
{
}

SDK& 
MultiImpl::addSession(const std::string& key, SessionHandler::Ptr handler) 
{ 
    auto res = mMap.emplace(key,std::make_pair(SDK(), SessionHandler::Ptr(handler)));
    if (!res.second) throw std::logic_error("MultiImpl: key already exists");

    SDK& sdk = res.first->second.first;

    sdk.setMessageHandler(std::bind(&SessionHandler::onMessage, handler.get(), &sdk, std::placeholders::_1));
    sdk.setStatusHandler(std::bind(&SessionHandler::onStatus, handler.get(), &sdk, std::placeholders::_1));
    sdk.setExceptionCallback(std::bind(&SessionHandler::onException, handler.get(), &sdk, std::placeholders::_1));
    sdk.setEngineReadyCallback(std::bind(&SessionHandler::onReady, handler.get(), &sdk));
    
    return sdk;
} 

SDK& 
MultiImpl::getSession(const std::string& key) 
{ 
    MultiSession::SessionMap::iterator it = mMap.find(key);
    if (it == mMap.end()) throw std::logic_error("MultiImpl: key doesn't exist");
    return it->second.first;
}

MultiSession::SessionMap&
MultiImpl::getSessions()
{
    return mMap;
}

void 
MultiImpl::removeSession(const std::string& key)
{
    mMap.erase(key);
}

void
MultiImpl::clear()
{
    mMap.clear();
}

void 
MultiImpl::createAll(const std::string& arrasUrl) 
{ 
    // arrasUrl ends in "/sessions" -- replace with "/multisession" to
    // get the multisession endpoint

    size_t p = arrasUrl.find_last_of('/',arrasUrl.size()-2);
    if (p == std::string::npos) {
        throw SDKException("Invalid arras url : "+arrasUrl);
    }
    std::string multiUrl = arrasUrl.substr(0,p) + "/multisession";

    // make a top level request object containing all the sessions

    PlatformInfo info;
    getPlatformInfo(info);
      
    const char* logname = std::getenv("LOGNAME");
    if (logname == nullptr) {
        throw SDKException("Unable to determine the current user's login name",
                              SDKException::GENERAL_ERROR);
    }

    Object requestObj;
    for (MultiSession::SessionMap::iterator it = mMap.begin();
         it != mMap.end(); ++it) {
        const std::string& key = it->first;
        SDK& sdk = it->second.first;
        if (sdk.isConnected()) continue; // skip connected SDKs
        SessionHandler::Ptr handler = it->second.second;
        ObjectRef subRequest = requestObj[key];
        sdk.mImpl->makeCreateRequest(handler->definition(),
                                     handler->options(),
                                     info, logname, subRequest);
        sdk.mImpl->setupMessageRecording(handler->definition());
    }

    // attempt a multisession create and process the response
    try { 
        HttpRequest req(multiUrl, POST);
        req.setUserAgent(USER_AGENT);
        std::string httpMsgBody = objectToString(requestObj);
        req.setContentType(network::APPLICATION_JSON);
        const HttpResponse& resp = req.submit(httpMsgBody);
        processCreateResponse(resp);
    } catch (const HttpException& e) {
        throw SDKException(e.what(), SDKException::GENERAL_ERROR);
        // sdk.cc uses AUTHENTICATION_ERROR, which is not necessarily correct.
    }
}

void
MultiImpl::sendAll(const api::MessageContentConstPtr& ptr,
                   api::ObjectConstRef options) const
{
    for (const auto& it: mMap) {
        const SDK& sdk = it.second.first;
        if (sdk.isConnected() && sdk.isEngineReady()) {
            sdk.sendMessage(ptr, options);
        }
    }
}

void
MultiImpl::processCreateResponse(const HttpResponse& resp)
{    
    // helper to extract the response from a multisession create
    // and connect to each session created.
    
    // extract response body as object
    std::string responseString; 
    bool haveError = false;
    std::string errorMessage;
    Object responseObject;

    if (resp.getResponseString(responseString)) {
        // got a string : is it JSON?
        try {
            stringToObject(responseString,responseObject);

            // its an object, does it have "error"
            if (responseObject.isMember("error")) {
                haveError = true;
                if (responseObject["message"].isString()) {
                    errorMessage = responseObject["message"].asString();
                } 
            }
        } catch (ObjectFormatError&) {
            // not an object : assume its an error message string
            haveError = true;
            errorMessage = responseString;
        }
    } else {
        // no response body
        haveError = true;
    }
    
    if (!haveError && resp.responseCode() == HTTP_OK) {

        // response object contains a response for every session
        // we asked to create
        for (ObjectIterator sessIt = responseObject.begin();
             sessIt != responseObject.end(); ++sessIt) {
            std::string key = sessIt.memberName();
            SDK& sdk = getSession(key);
            try {
                sdk.mImpl->connectSession(*sessIt);
            } catch (SDKException& ce) {
                // prefix exception message with SDK key string
                std::string msg("["+key+"] ");
                msg += ce.what();
                throw SDKException(msg,ce.getType());
            }
        }

    } else if (resp.responseCode() == HTTP_SERVICE_UNAVAILABLE) {
        // don't care about error message : status code is enough to tell us
        // what happened...
        throw SDKException("Insuffient resources available to fill this request",
                              SDKException::NO_AVAILABLE_RESOURCES_ERROR);
    } else {
        // unknown status code must be an error
        std::string err = std::string("Server responded with error code ") + std::to_string(resp.responseCode());
        if (!errorMessage.empty())
            err += ", message: " + errorMessage;
        throw SDKException(err,SDKException::GENERAL_ERROR);
    }   
}

bool 
MultiImpl::allConnected() const 
{
    for (MultiSession::SessionMap::const_iterator it = mMap.begin();
         it != mMap.end(); ++it) {
        const SDK& sdk = it->second.first;
        if (!sdk.isConnected()) return false;
    }
    return true;
}

bool 
MultiImpl::allDisconnected() const 
{ 
    for (MultiSession::SessionMap::const_iterator it = mMap.begin();
         it != mMap.end(); ++it) {
        const SDK& sdk = it->second.first;
        if (!sdk.isDisconnected()) return false;
    }
    return true;
}

bool MultiImpl::allReady() const 
{ 
    for (MultiSession::SessionMap::const_iterator it = mMap.begin();
         it != mMap.end(); ++it) {
        const SDK& sdk = it->second.first;
        if (!sdk.isEngineReady()) return false;
    }
    return true; 
}

bool MultiImpl::waitForAllReady(unsigned maxSeconds) const 
{ 
    while (maxSeconds > 0) {
        bool allReady = true;
        for (MultiSession::SessionMap::const_iterator it = mMap.begin();
             it != mMap.end(); ++it) {
            const SDK& sdk = it->second.first;
            if (sdk.isErrored() || !sdk.isConnected())
                return false;
            if (!sdk.isEngineReady()) {
                allReady = false;
                break;
            }
        }
        if (allReady) return true; 
        std::this_thread::sleep_for(std::chrono::seconds(1));
        maxSeconds--;
    }
    return allReady();
}

bool MultiImpl::waitForAllDisconnected(unsigned maxSeconds) const 
{    
    while (maxSeconds > 0) {
        if (allDisconnected()) return true;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        maxSeconds--;
    }
    return allDisconnected();
}

void MultiImpl::disconnectAll() 
{ 
    for (MultiSession::SessionMap::iterator it = mMap.begin();
         it != mMap.end(); ++it) {
        SDK& sdk = it->second.first;
        sdk.disconnect();
    }
}

void MultiImpl::shutdownAll() 
{ 
    for (MultiSession::SessionMap::iterator it = mMap.begin();
         it != mMap.end(); ++it) {
        SDK& sdk = it->second.first;
        sdk.shutdownSession();
    }
}

} // end namespace internal

MultiSession::MultiSession() : mImpl(new internal::MultiImpl) {}
MultiSession::~MultiSession() {} 
std::string MultiSession::requestArrasUrl(const std::string& dc, const std::string& env) 
{ return client::Client::getArrasUrl_static(dc,env); }
SDK& MultiSession::addSession(const std::string& key, SessionHandler::Ptr handler) { return mImpl->addSession(key,handler); }
SDK& MultiSession::getSession(const std::string& key) { return mImpl->getSession(key); }
MultiSession::SessionMap& MultiSession::getSessions() { return mImpl->getSessions(); }
void MultiSession::removeSession(const std::string& key) { mImpl->removeSession(key); }
void MultiSession::clear() { mImpl->clear(); }
void MultiSession::createAll(const std::string& arrasUrl) { mImpl->createAll(arrasUrl); }
void MultiSession::sendAll(const api::MessageContentConstPtr& ptr, api::ObjectConstRef options) const { mImpl->sendAll(ptr, options); };
bool MultiSession::allConnected() const { return mImpl->allConnected(); }
bool MultiSession::allDisconnected() const { return mImpl->allDisconnected(); }
bool MultiSession::allReady() const { return mImpl->allReady(); }
bool MultiSession::waitForAllReady(unsigned maxSeconds) const { return mImpl->waitForAllReady(maxSeconds); }
bool MultiSession::waitForAllDisconnected(unsigned maxSeconds) const { return mImpl->waitForAllDisconnected(maxSeconds); }
void MultiSession::disconnectAll() { mImpl->disconnectAll(); }
void MultiSession::shutdownAll() { mImpl->shutdownAll(); }

    }
}

