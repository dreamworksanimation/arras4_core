// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_LOCAL_SESSION_H__
#define __ARRAS4_LOCAL_SESSION_H__

#include <message_api/Object.h>
#include <message_api/Address.h>

#include <execute/SpawnArgs.h>
#include <execute/Process.h>
#include <execute/ShellContext.h>

#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

namespace arras4 {
    namespace impl {
	class ProcessManager;
    }
    namespace network {
	class Peer;
    }

    namespace client {

class ComputationDefaults;

class LocalSession : public impl::ProcessObserver
{
public:
    // called when the session terminates,
    // passing: bool expected (was termination triggered by calling stop())
    //          Object data (contains termination reason)
    using TerminateFunc = std::function<void(bool,api::ObjectConstRef)>;
    
    LocalSession(impl::ProcessManager& procman, const std::string& aSessionId);
    ~LocalSession();

    // throws SessionError
    void setDefinition(api::ObjectConstRef def);
    // throws SessionError
    
    // must pass a shared pointer to 'this' (seems
    // safer than shared_from_this, which would be undefined if
    // there were no pre-existing shared ptr)
    void start(std::shared_ptr<LocalSession> shared_this,
	       TerminateFunc tf);

    // throws SessionError
    void stop();

    void pause();
    void resume();

    // removes terminate callback, so that client can be safely deleted.
    void abandon();

    const api::Address& address() { return mAddress; }
    std::shared_ptr<network::Peer> peer() { return mPeer; }

    
 
    // ProcessObserver
    void onTerminate(const api::UUID& id,
		     const api::UUID& sessionId,
		     impl::ExitStatus status);
    void onSpawn(const api::UUID& id,
		 const api::UUID& sessionId,
		 pid_t pid);

private:
    api::ObjectConstRef getObject(api::ObjectConstRef obj,
				  const std::string& key);
    void buildRouting(api::ObjectConstRef clientDef);
    void processComputation(const std::string& name, api::ObjectConstRef definition, api::ObjectConstRef contexts);
    void applyPackaging(api::ObjectConstRef definition, api::ObjectConstRef context = api::Object());
    void applyNoPackaging(api::ObjectConstRef ctx);
    void applyCurrentEnvironment(api::ObjectConstRef ctx);
    void applyShellPackaging(impl::ShellType type,api::ObjectConstRef ctx);
    void applyRezPackaging(unsigned rezMajor,api::ObjectConstRef ctx);
    bool writeConfigFile();
    void spawnProcess();
    void connectProc();
    void readRegistration();

    api::Address mAddress;
    std::string mName;
    impl::ProcessManager& mProcessManager;

    impl::SpawnArgs mSpawnArgs;
    api::Object mExecConfig;
    api::Object mRouting;
    std::string mExecConfigFilePath;

    std::shared_ptr<impl::Process> mProcess;
    std::atomic<bool> mTerminationExpected{false};
    TerminateFunc mTerminateCallback;
    std::mutex mCallbackMutex;

    std::string mIpcAddress;
    std::shared_ptr<network::Peer> mPeer;
    std::thread mConnectThread;
    std::mutex mConnectMutex;
    std::condition_variable mConnectCondition;
    bool mConnecting = false;
};

}
}
#endif

