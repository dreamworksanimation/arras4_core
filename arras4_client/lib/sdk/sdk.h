// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/** \mainpage Arras Client API
 * The main entry point into Arras is via the arras::sdk::SDK class.
 * Please see http://mydw.anim.dreamworks.com/display/RD/Arras+4.0
 * for more information regarding Arras.
 *
 */

#pragma once

#include <arras4_athena/AthenaLogger.h>
#include <client/api/AcapAPI.h>
#include <client/api/ClientException.h>
#include <client/api/SessionDefinition.h>
#include <exception>
#include <functional>
#include <memory>
#include <message_api/messageapi_types.h>

#include <message_impl/PeerMessageEndpoint.h>

namespace arras4 {
    namespace sdk {
        typedef client::AcapResponse AcapResponse;
        typedef client::AcapRequest AcapRequest;

        namespace internal {
            class Impl;
            class MultiImpl;
        }


typedef arras4::client::ClientException SDKException;

/**
 * Class enscapsulating the SDK Client
 */
class SDK
{
public:
    SDK();
    ~SDK();
    SDK(const SDK& other) = delete;
    SDK(SDK&&) = default;

    /**
     * Callback for handling incoming Messages from the service
     * Function signature takes a Message reference
     */
    typedef std::function<void(const api::Message&)> MessageHandler;
    
    /**
     * Callback for handling Status repsonses from the service
     * Function signature takes a string reference, which contains the status as a JSON string
     */
    typedef std::function<void(const std::string&)> StatusHandler;
    
    /**
     * Callback for handling responding to exceptions from the service
     * Function signature takes an exception reference
     */
    typedef std::function<void(const std::exception&)> ExceptionCallback; // keep in synch with Client.h

     /**
     * Callback for handling 'engine ready' notification
     */
    typedef std::function<void()> EngineReadyCallback; // keep in synch with Client.h

    /**
     * Sends a Message to the service
     * @param msgPtr A shared pointer to a Message
     */
    void sendMessage(const api::MessageContentConstPtr& ptr,
                     api::ObjectConstRef options=api::Object()) const;


    void setAsyncSend();
    void setSyncSend();

    /**
     * Requests the url for ACAP from the Studio Config Service.
     * @param datacenter Lower case three letter acronym for the studio.
     * @param environment PSO service environment, typical values are uns (unstable), stb (stable)
     * and prod (production)
     * @throw SDKException type=CONNECTION_ERROR when the Studio's Config Service returns an
     * HTTP status code other than 200=OK, or the underlying third party HTTP lib (currently libcurl) 
     * returns an error code.or the DWA_CONFIG_ENV_NAME environment variable is undefined..
     * @return url for ACAP suitable for passing in to requestSession() and selectSession() methods.
     */
    const std::string requestArrasUrl(const std::string& datacenter="gld", const std::string& environment="prod");


    /** Creates a session on the service, and connects to Arras using this session.
     * @param aDefinition Session definition object describing the session
     * @param url The ACAP endpoint to make the request against
     * @param sessionOptions Optional SessionOptions instance containing session metadata such as:
     * production, sequence, shot, department and team
     * @return ACAP response, providing any error condition and the session id.
     *  @throw SDKException type=NO_AVAILABLE_RESOURCES_ERROR when there are not enough resources (cores or memory) 
     * available in the Arras pool to satisfy the requirements of the requested session.
     * @throw SDKException type=CONNECTION_ERROR or GENERIC_ERROR when the ACAP service returns an HTTP status code
     * other than 200=OK
     */
    std::string createSession(const client::SessionDefinition& aDefinition,
                              const std::string& acapUrl,
                              const client::SessionOptions& sessionOptions=client::SessionOptions());

    const std::string& sessionId() const;
    bool isConnected() const;
    bool isDisconnected() const;
    bool isEngineReady() const;
    bool isErrored() const;
    bool waitForEngineReady(unsigned maxSeconds) const;
    bool waitForDisconnect(unsigned maxSeconds) const;

    /**
     * Shuts down the session, causing a session status message to be sent
     * followed by disconnect.Typically follow with "waitForDisconnect"
     **/
    void shutdownSession();
    
    /**
     * Disconnects (abruptly) from the service.
     */
    void disconnect();

    /** 
      * Pause execution of all computations in the session
      */
    void pause();

    /** 
      * Resume execution of all computations in the session
      */
    void resume();

    /**
     * Sets the callback for incoming messages.
     * This callback is invoked whenever a messsage is received by the SDK client
     * @param messageHandler The message handling callback
     */
    void setMessageHandler(MessageHandler messageHandler);
    
    /**
     * Sets the callback for status messages.
     * This callback is invoked whenever a session status messages is sent by the service
     * @param statusHandler The session status handling callback
     */
    void setStatusHandler(StatusHandler statusHandler);
    
    /**
     * Sets the callback for exceptions.
     * This callback is invoked whenever an exception is thrown by the SDK, letting the client
     * respond to it as needed
     * @param exceptionCallback The exception handling callback
     */
    void setExceptionCallback(ExceptionCallback exceptionCallback);
  
    /**
     * Sets the callback for 'engine ready'
     * This callback is invoked when the session 'engine ready' notification arrives
     */
    void setEngineReadyCallback(EngineReadyCallback readyCallback);

    /**
     * Disable message chunking
     */  
    void disableMessageChunking();

    /** 
     * Enable message chunking
     * 0 for either value means use previous setting 
     * (or default if no previous setting)
     **/
    void enableMessageChunking(size_t minChunkingSize = 0,
                               size_t chunkSize = 0);

    /**
     * Query if a given session exists.
     * using local server if datacenter or environment is empty
     * @param sessionId, input session id string to query
     * @param datacenter, input string, defaults to "gld"
     * @param environment, input string, defaults to "prod"
     * @returns true of the specified session exists
     */
    bool sessionExists(const std::string& sessionId,
    		const std::string& datacenter="gld",
    		const std::string& environment="prod") const;
       
    typedef std::shared_ptr<SDK> Ptr;

    static bool configAthenaLogger(const std::string& athenaEnv = "prod",
                                   bool useColor=true,
                                   const std::string& syslogHost = "localhost",
                                   unsigned short syslogPort = log::SYSLOG_PORT);

    // direct access to the message endpoint : for expert use only...
    impl::PeerMessageEndpoint* endpoint();

    // resolve rez settings, replacing "rez_packages" with "rez_context"
    // returns context, or "ok" if there was nothing to resolve, or
    // "error" if resolution failed (in which case err has details)
    static std::string resolveRez(api::ObjectRef rezSettings,
				  std::string& err);

    // resolve rez settings for an entire session.
    // return true if successful, otherwise err contains an error message)
    static bool resolveRez(client::SessionDefinition& def,
			   std::string& err);

    // progress update with percent complete (status always pending)
    void progress(const std::string& stage, unsigned percent);
    // progress update with text
    void progress(const std::string& stage,
		  const std::string& status = "pending",
		  const std::string& text=std::string());
    // add info to progress ui
    void progressInfo(const std::string& category,api::ObjectConstRef value);

    // set the command to auto start an external progress UI
    static void setProgressAutoExecCmd(const std::string& cmd);
    // set the channel name for progress messages
    static void setProgressChannel(const std::string& channel);

private:
    friend class internal::MultiImpl;
    std::unique_ptr<internal::Impl> mImpl;

};

} // end namespace sdk
} // end namespace arras

