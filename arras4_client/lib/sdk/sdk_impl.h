// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// this header is required by MultiSDK.cc, to give it access to internal SDK/Client functions
#pragma once

#include "sdk.h"

#include <client/api/Client.h>
#include <client/api/Component.h>
#include <message_api/Message.h>

#include <string>
#include <memory>
#include <mutex>

namespace arras4 {

namespace sdk {
namespace internal {

typedef arras4::client::Client Client;

class Impl {
public:

    Impl();
    ~Impl();
    void setAsyncSend(bool flag);
    void sendMessage(const api::MessageContentConstPtr& msgPtr,
                     api::ObjectConstRef options) const;
    const std::string requestArrasUrl(const std::string& datacenter, const std::string& environment);
    bool sessionExists(const std::string& aSessionId, const std::string& datacenter, const std::string& environment) const;
    std::string createSession(const client::SessionDefinition& sessionDef, const std::string& url,
                              const client::SessionOptions& sessionOptions);

    const std::string& sessionId() const;
    void shutdownSession();
    void disconnect();
    bool isConnected() const;
    bool isDisconnected() const;
    bool isEngineReady() const;
    bool isErrored() const;
    bool waitForEngineReady(unsigned maxSeconds) const;
    bool waitForDisconnect(unsigned maxSeconds) const;
    void pause();
    void resume();
    void setMessageHandler(SDK::MessageHandler handler);
    void setStatusHandler(SDK::StatusHandler handler);
    void setExceptionCallback(SDK::ExceptionCallback handler);
    void setEngineReadyCallback(SDK::EngineReadyCallback handler);
    void disableMessageChunking();
    void enableMessageChunking(size_t minChunkingSize,size_t chunkSize);

    static bool configAthenaLogger(const std::string& athenaEnv = "prod",
                                   bool useColor=true,
                                   const std::string& syslogHost = "localhost",
                                   unsigned short syslogPort = log::SYSLOG_PORT);

    // for use by MultiSDK

    /// make the request body for a create request
    void makeCreateRequest(const client::SessionDefinition& aDefinition, 
                           const client::SessionOptions& aSessionOptions,
                           const impl::PlatformInfo& info,
                           const std::string& username,
                           api::ObjectRef reqObj);

    /// connect using the response to a create request
    void connectSession(api::ObjectConstRef responseObject);

    /// set the session up for message recording based
    /// on the options under "(client)" in the
    /// session definition
    void setupMessageRecording(const client::SessionDefinition& aDefinition); 

    //// direct access to the message endpoint : for expert use only...
    impl::PeerMessageEndpoint* endpoint();

    // progress update with percent complete
    void progress(const std::string& stage, unsigned percent);
    // progress update with text
    void progress(const std::string& stage, 
		  const std::string& status,
		  const std::string& text);
    // add info to progress ui
    void progressInfo(const std::string& category,api::ObjectConstRef value);

private:
    class ComponentWrapper : public client::Component {
    public:
        ComponentWrapper(Impl& impl) : Component(*impl.mClient), mImpl(impl) { }
        ~ComponentWrapper() {}
        void onMessage(const api::Message& aMsg);
        void onStatusMessage(const api::Message& aMsg);
        void onEngineReady();
    private:
        Impl& mImpl;
    };

    void ExceptionCallbackWrapper(const std::exception& error);

    friend class ComponentWrapper;
    typedef std::unique_ptr<AcapResponse> AcapResponsePtr;
    typedef std::unique_ptr<ComponentWrapper> ComponentWrapperPtr;
    typedef std::unique_ptr<Client> ClientPtr;

    std::mutex mConnectionMutex;

    ClientPtr mClient;
    ComponentWrapperPtr mComponent;
    SDK::MessageHandler mHandler;
    SDK::StatusHandler mStatusHandler;
    SDK::ExceptionCallback mExceptionCallback;
    SDK::EngineReadyCallback mEngineReadyCallback;

    static std::mutex mLoggerMutex;
    static bool mCreatedLogger;
};

}
}
}
