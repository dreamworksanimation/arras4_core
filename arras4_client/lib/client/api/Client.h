// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CLIENT_H__
#define __ARRAS4_CLIENT_H__

#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "AcapAPI.h"
#include "ProgressSender.h"

#include <client/local/LocalSessions.h>

#include <message_api/messageapi_types.h>
#include <message_api/Object.h>
#include <network/network_types.h>
#include <chunking/ChunkingConfig.h>
#include <shared_impl/MessageQueue.h>

namespace arras4 {
    namespace impl {
        class PeerMessageEndpoint;
        class MessageEndpoint;
        class PlatformInfo;
    }
}

namespace arras4 {
    namespace client {
        
        class Component;
        class SessionDefinition;

class Client
{
public:
    typedef std::function<void(const std::exception&)> ExceptionCallback;


    static impl::ProcessManager& processManager();
    static LocalSessions& localSessions();
    static ProgressSender& progressSender();

    Client();
    Client(const std::string& aUserAgent);
         

    Client(Client&)=delete;
    Client& operator=(Client&)=delete;
    ~Client();

    // call before connecting...
    void setAsyncSend(bool flag) { mSendAsync = flag; }

    enum State {
        // idle, not connected, not trying to connect
        STATE_DISCONNECTED,
        // sent session selection and client platform info, got TCP connection info and session ID from service and
        STATE_CONNECTING,
        // successfully connected to service via TCP
        STATE_CONNECTED,
        // shutting down the TCP connection
        STATE_DISCONNECTING,
    };

    /// List of Session names
    typedef std::vector<std::string> SessionList;

    /// Unique ID for ACAP Sessions
    typedef std::string AcapSessionID;
 
    const std::string requestArrasUrl(const std::string& datacenter="gld", const std::string& environment="prod");
    static const std::string getArrasUrl_static(const std::string& datacenter="gld", 
                                                const std::string& environment="prod",
                                                const std::string& userAgent = defaultUserAgent);

    /// return true if the given sessionId is found in the arras coordinator
    /// using local server if given empty datacenter or environment.
    /// @param sessionId, input string, session id to query
    /// @param datacenter, input string, defaults to "gld"
    /// @param environment, input string, defaults to "prod"
    bool sessionExists(const std::string& sessionId,
                       const std::string& datacenter="gld",
                       const std::string& environment="prod") const;

    // Connects to a session by name, optionally providing session overrides and production details
    std::string createSession(const SessionDefinition& aDefinition, 
                              const std::string& aUrl,
                              const SessionOptions& aSessionOptions=SessionOptions());
                 
    /// Disconnect the Arras socket
    void disconnect();

    // send the shutdown message which will cause the server
    // to shut down the session and send a status message
    void sendShutdownMessage();

    /// Returns true if the Arras socket is connected, and not
    /// waiting for a pending disconnect (due to a shutdown message)
    bool isConnected() const;

    /// Returns true if the Arras socket is disconnected
    bool isDisconnected() const;

    /// Returns true if 'engineReady' has been received
    bool isEngineReady() const;
    
    /// Returns true if an error has occurred on this connection
    bool isErrored() const;

    /// Pause all computations
    void pause();
    void resume();

    /// Blocks current thread until engine is ready
    /// or 'maxSeconds' has expired. Returns true if
    /// engine is ready
    bool waitForEngineReady(unsigned seconds) const;

    /// Blocks current thread until disconnected
    /// or 'maxSeconds' has expired. Returns true if
    /// engine is ready
    bool waitForDisconnect(unsigned seconds) const;

    /// Adds a callback for responding to any exceptions
    void addExceptionCallback(const ExceptionCallback& callback);

    /// Removes an existing registered callback (@see addExceptionCallback)
    void removeExceptionCallback(const ExceptionCallback& callback);


    /// Adds a component that is used to respond to incoming messages
    void addComponent(Component* aComponent);

    /// Removes a previously registered component (@see addComponent)
    void removeComponent(Component* aComponent);

    /// Returns the list of valid session names
    SessionList sessionNames() const { return mSessionDefinitions; }

    /// Returns the number of available sessions
    int numSessionDefinitions() const;

    /// Returns the session name given an index into the session list
    const char* sessionKey(int aIdx);

    /// return Arras session ID string
    const std::string& sessionId() const { return mSessionId; }

    /// Sends a Message, returning the number of bytes sent
    size_t send(const api::MessageContentConstPtr& content,
                api::ObjectConstRef options = api::Object());

    /// Message chunking settings
    // 0 means use previous setting (or default if no previous setting)
    void disableMessageChunking();
    void enableMessageChunking(size_t minChunkingSize = 0,
                               size_t chunkSize = 0);

 
    /// make the request body for a create request
    void makeCreateRequest(const SessionDefinition& aDefinition, 
                           const SessionOptions& aSessionOptions,
                           const impl::PlatformInfo& info,
                           const std::string& username,
                           api::ObjectRef reqObj);

    /// connect using the response to a create request
    void connectSession(api::ObjectConstRef responseObject);

    /// set the session up for message recording based
    /// on the options under "(client)" in the
    /// session definition
    void setupMessageRecording(const SessionDefinition& aDefinition);

    impl::PeerMessageEndpoint* endpoint();

    // progress update with percent complete
    void progress(const std::string& stage, unsigned percent);
    // progress update with text
    void progress(const std::string& stage, 
		  const std::string& status = "pending",
		  const std::string& text=std::string());
    // add info value
    void progressInfo(const std::string& category,
		      api::ObjectConstRef value);

protected:
    void setState(State state) { mState = state; }
    State getState() const { return mState; }

    std::list<Component*> mComponents;

    /// Hostname of the ACAP or socket peer
    std::string mHostname;
    std::string mHostIp;

    /// Port of the socket peer
    unsigned short mPort;

    // primary communication with Arras service
    std::shared_ptr<arras4::network::Peer> mPeer;
    std::string mSessionId;
    std::atomic<State> mState;

    // we have received some error and our connection is invalid
    std::atomic<bool> mConnectionError;

    // list of session keys (this is temp)
    SessionList mSessionDefinitions;
    AcapSessionID mAcapSessionId;

    // Error handling callback
    std::list<ExceptionCallback> mExceptionCallbacks;

    std::string mUserAgent;

    // id used for communication with the progress GUI
    std::string mProgressId;

private:

    static const std::string defaultUserAgent;

    // connect to service via raw TCP
    void connectTCP();

    // Post connection setup
    void postConnect();

    // actually deliver pending messages to registered components
    void deliverMessages();

    // this method blocks for incoming messages
    void threadProc();

    void sendProc();

    // Closes the connection without changing the state
    void shutdownConnection();

    size_t sendAsync(const api::MessageContentConstPtr& content,
                    api::ObjectConstRef options = api::Object());
    size_t sendSync(const api::MessageContentConstPtr& content,
                    api::ObjectConstRef options = api::Object());

    // Adds the attribute to the post body if the value is not empty, adds null otherwise
    void addHTTPBodyAttr(api::ObjectRef aBody, const std::string& aName, const std::string& aValue) const;
    void addHTTPBodyAttr(api::ObjectRef aBody, const std::string& aName, api::ObjectConstRef aValue) const;

    /// Query the dwa studio configuration service for the given resource
    /// @param resourcepath, input string, path of resource
    /// @param datacenter, input string, "gld" or "las"...
    /// @param environment, input string, "prod" or "uns"...
    const std::string getResourceFromConfigurationService(
        const std::string& resourcepath,
        const std::string& datacenter,
        const std::string& environment) const;
  
    static const std::string getResourceFromConfigurationService_static(
        const std::string& resourcepath,
        const std::string& datacenter,
        const std::string& environment,
        const std::string& userAgent = defaultUserAgent);

    /// Returns the coordinator endpoint.
    const std::string getCoordinatorEndpoint(
        const std::string& datacenter="gld",
        const std::string& environment="prod") const;

    std::string createDistributed(const SessionDefinition& aDefinition, 
				  const std::string& aUrl,
				  const SessionOptions& aSessionOptions);
    void shutdownDistributed();
    std::string createLocal(const SessionDefinition& aDefinition,
			    const SessionOptions& aSessionOptions);
    void shutdownLocal();
    void localTermination(bool expected,api::ObjectConstRef data);
    void logLocal(const SessionDefinition& aDefinition,
		  const SessionOptions& aSessionOptions);

    void registerDisconnect(bool expected,
			    const std::string& message);

    // thread that runs threadProc()
    std::thread mThread;
                
    // set to true when an 'engineReady' message is received
    std::atomic<bool> mEngineReady;

    // control value that determines if threadProc() runs or exits
    std::atomic<bool> mRun;

    bool mSendAsync;
    std::thread mSendThread;
    impl::MessageQueue* mOutgoingQueue = nullptr;

    // message endpoint for send/recv
    impl::PeerMessageEndpoint* mPeerEndpoint = nullptr;
    impl::MessageEndpoint* mMessageEndpoint = nullptr;

    impl::ChunkingConfig mChunkingConfig;

    // directories for message recording (if empty,
    // no recording occurs)
    std::string mIncomingSaveDir;
    std::string mOutgoingSaveDir;

    bool mIsLocal;

    // Arras url used for original connection
    std::string mArrasUrl;

};

}
}
#endif
