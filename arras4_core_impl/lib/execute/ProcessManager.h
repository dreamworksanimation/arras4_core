// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PROCESS_MANAGER_H__
#define __ARRAS4_PROCESS_MANAGER_H__

#include "Process.h"
#include "MemoryTracking.h"

#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace arras4 {
    namespace impl {

        class ControlGroup;
        class ProcessController;

// Manages a set of processes. Should only be used as a singleton, because
// the exit monitor thread reaps all child processes when they terminate, whether
// they were created by this manager or not.
//
// Processes have a "sessionId", but it is only used here for logging 
// purposes : ProcessManager doesn't really know about sessions.
// Supports:
//   -- exit tracking : When a spawned process exits it will be automatically
//   moved to "Terminated" state
//   -- cgroups : Each OS process can belong to a subgroup named using the process uuid.
//      Can be configured to enforce the assigned memory and cpu limits in Process
//      Can support "loans" : if a process overruns its assigned memory and there
//      is unused memory the offending process is given an extra lease of life to continue;
//      however if may be terminated at any point if another process requires the memory
//      as part of its assignment.
//   -- controlled processes. Processes may connect back via NodeRouter, giving a "control
//      channel" that allows extended operations. This is implemented in ProcessController.
//      Currently the only control operation used by ProcessManager itself is "stop",  used 
//      as the first port of call to terminate a controlled process (of course, in reality 
//      "controlled process" == "computation" and the control channel is just the regular IPC 
//      message socket)
class ProcessManager
{
public:
    ProcessManager(unsigned availableMemoryMb = 0,
		   bool useCgroups = false,
                   bool enforceMemory = true,
                   bool enforceCpu = true,
                   bool loanMemory = false);
    ~ProcessManager();

    void setProcessController(std::shared_ptr<ProcessController> controller);

    Process::Ptr addProcess(const api::UUID& id = api::UUID::generate(),
                            const std::string& name = std::string(),
                            const api::UUID& sessionId = api::UUID::null);

    Process::Ptr getProcess(const api::UUID& id);
    
    // terminate process and remove from manager. Uses fast termination (SIGKILL), but
    // may block for a while waiting for the process to stop.
    // returns false if process doesn't exist
    bool removeProcess(const api::UUID& id);

private:
 
    // get process when mProcessesMutex is already locked
    Process::Ptr getProcess_wlock(const api::UUID& id);
    friend class Process;

    // spawn callbacks : these are called from Process during 
    // spawning. most are called from the main (parent) process, 
    // except postForkChild_cb,which is called from the child between 
    // fork() and exec(). These are used to help support
    // the exit monitor and cgroups. The process state mutex is
    // held during these calls, and so you should not call
    // functions that lock it : for example, get the process pid
    // using ".mPid", not ".pid()"...

    // called from main process just before forking
    void preFork_cb(Process& p);
    // called from main process after fork has failed.
    void failedFork_cb(Process& p);
    // called from main process after fork has succeeded
    void postFork_cb(Process& p);
    // called from child process after fork has succeeded
    void postForkChild_cb(Process& p);

    // called from exitMonitorThread when a spawned process
    // exits
    void exit_cb(Process& p);

    // called from main process to request a controlled stop
    // return true if stop is in progress, false if
    // unavailable
    bool controlledStop(Process& p);

    // subgroups
    void initControlGroups();
    Process::Ptr sgNameToProcess(const std::string& name);
    void createControlSubgroup(Process& p);
    void addChildToSubgroup(Process& p);
    void destroyControlSubgroup(Process& p);
    
    // memory
    void reserveMemory(Process& p);
    void releaseMemory(Process& p);
    Process::Ptr findBiggestBorrower(unsigned& borrowedAmount);

    // runs in exitMonitorThread to detect child process exit
    void exitMonitorProc();
    void handleChildExit(pid_t pid,int status);

    // runs in oomMonitorThread to detect out-of-memory conditions
    void oomMonitorProc();
    void handleOomCondition(const std::vector<std::string>& groups);


    std::atomic<bool> mRunThreads;
    std::thread mExitMonitorThread;
    std::thread mOomMonitorThread;

    // holds all processes managed by me
    // mPidToProcess is used while a thread is running,
    // to map OS events back to the Process object
    mutable std::mutex mProcessesMutex;
    std::map<api::UUID,Process::Ptr> mProcesses;
    std::map<pid_t,Process::Ptr> mPidToProcess;

    // use cgroups to control and detect resource
    // violations
    std::shared_ptr<ControlGroup> mControlGroup;
    bool mUseControlGroups;
    bool mEnforceMemory;
    bool mLoanMemory;
    bool mEnforceCpu;

    // counts total memory in use, both assigned to
    // processes and borrowed by processes that
    // have exceeded their assignment
    MemoryTracking mMemory;

    // messaging connection to computations, via 
    // NodeRouter
    std::shared_ptr<ProcessController> mProcessController;

};
    
    }
}
#endif

