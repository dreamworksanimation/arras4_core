// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "sdk.h"

#include <message_api/messageapi_types.h>

#include <map>
#include <memory>
#include <string>
#include <utility> // std::pair

namespace arras4 {
    namespace sdk {

/** provides definition, options and handlers for a session **/
class SessionHandler
{
public:
    SessionHandler(const client::SessionDefinition& def,
                   const client::SessionOptions& opts = client::SessionOptions())
        : mDefinition(def),
          mOptions(opts)
        {}

    virtual ~SessionHandler() {}

    client::SessionDefinition& definition() { return mDefinition; } 
    const client::SessionDefinition& definition() const { return mDefinition; }
    client::SessionOptions& options() { return mOptions; }
    const client::SessionOptions& options() const { return mOptions; }

    virtual void onMessage(SDK*, const api::Message&) {}
    virtual void onStatus(SDK*, const std::string&) {}
    virtual void onException(SDK*, const std::exception&) {}
    virtual void onReady(SDK*) {}
    
    using Ptr = std::shared_ptr<SessionHandler>;

private:
    client::SessionDefinition mDefinition;
    client::SessionOptions mOptions;
};

namespace internal {
    class MultiImpl;
}

/* A container for multiple SDK objects, accessed by string key.
 * All the SDK objects in a MultiSession container can be created in a single 
 * transaction, by calling createAll
 */
class MultiSession
{
public:
    MultiSession();
    ~MultiSession();
    MultiSession(const MultiSession& other) = delete;

    using SessionEntry = std::pair<SDK, SessionHandler::Ptr>;
    using SessionMap = std::map<std::string, SessionEntry>;

    /** request an arras url to pass in to createAll() : essentially the same as the SDK function
     * (note this is a non-static member function for consistency with SDK, but has a static 
     * implementation)
     */
    std::string requestArrasUrl(const std::string& datacenter="gld", 
                                const std::string& environment="prod");
    /**
     * add a new SDK instance that can later be retrieved with "key"
     *
     * The new instance will initially be disconnected (like any newly created SDK instance),
     * but when "createAll" is called, the definition and options in "handler" will be used 
     * to create this SDK's session and connect to it.
     *
     * The SDK instance has its handler functions assigned to call member functions in 
     * 'handler' when it is added. You can reassign them directly after adding if desired : 
     * -- they won't be modified again by MultiSession.
     *
     * If an SDK has already been added with the given key std::logic_error is thrown. 
     * You can replace an SDK explicitly by calling "remove" followed by "add".
     */
    SDK& addSession(const std::string& key,
                SessionHandler::Ptr handler); 
    
    /** get an SDK instance previously added using "addSDK". Throws std::logic_error if there is
        no SDK with the given key**/
    SDK& getSession(const std::string& key);

    /** Returns a reference to a map containing all of the sessions & session handlers **/
    SessionMap& getSessions();

    /** remove an SDK instance previously added using "addSDK". The existing SDK instance
        will be destroyed : if no SDK has been added under key, nothing happens **/
    void removeSession(const std::string& key);

    /** remove all SDK instances **/
    void clear();

    /**
     * create and connect sessions to all SDKs in the container, using for each the definition 
     * and options that were provided to its addSDK() call. Any sessions that are currently connected 
     * will be left alone -- connected sessions that you want to recreate should be 
     * disconnected separately prior to making this call.
     *
     * The creation portion of the operation occurs in a single transaction : either all the sessions
     * are successfully created and started or none of them are. Following successful creation, the
     * function attempts to connect each session. Failure of any of these attempts will cause an
     * exception to be thrown by createAll(), but the function does not disconnect SDKs that have already 
     * successfully connected. You can call "disconnectAll()" or "shutdownAll()" in an exception handler
     * if this is desired behavior.
     */
    void createAll(const std::string& arrasUrl);

    /**
     * Sends the same message to all sessions that are ready
     */
    void sendAll(const api::MessageContentConstPtr& ptr,
                 api::ObjectConstRef options=api::Object()) const;

    /**
     * Return true if all SDKs in the container are connected *
     */
    bool allConnected() const;
 
    /**
     * Return true if all SDKs in the container are disconnected *
     */
    bool allDisconnected() const;

    /**
     * Return true if all SDKs in the container are ready *
     */
    bool allReady() const;

    /**
     * wait until all SDKs in the container are ready, for up to 'maxSeconds'.
     * if any sdk becomes errored or disconnected, or the time elapses, return false
     **/
    bool waitForAllReady(unsigned maxSeconds) const;

    /**
     * wait until all SDKs in the container are disconnected, for up to 'maxSeconds'
     **/
    bool waitForAllDisconnected(unsigned maxSeconds) const;

    /**
     * disconnects all sessions that are currently connected.
     **/
    void disconnectAll();
    /**
     * calls shutdownSession on every connected session in the container.
     **/
     void shutdownAll();
   

private:
    std::unique_ptr<internal::MultiImpl> mImpl;
};


} // end namespace sdk
} // end namespace arras

