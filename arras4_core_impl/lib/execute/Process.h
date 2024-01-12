// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PROCESS_H__
#define __ARRAS4_PROCESS_H__

#include <message_api/UUID.h>

#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>

#include <sys/types.h>

namespace arras4 {
    namespace impl {

class ProcessManager;
class SpawnArgs;

// possible states of a process
enum class ProcessState
{
    NotSpawned,    // local process hasn't been created
    Spawned,       // process has been created, pid is now valid
    Terminating,   // termination is in progress, pid will become invalid
    Terminated     // terminated, pid is invalid
};

// result of an attempted state change
enum class StateChange
{
    Success,     // operation succeeded, state was changed
    Achieved,    // process had already achieved desired state
    InProgress,  // state will be achieved via an async thread
    Invalid,     // illegal state change was requested
    Terminated   // operation failed, and process was terminated
};

// these generally count as successes
inline bool StateChange_success(StateChange sc) 
{
    return sc == StateChange::Success || 
        sc == StateChange::Achieved || 
        sc == StateChange::InProgress;
}

// Records the way a process arrived at state Terminated
// If the process was successfully spawned, terminated
// and reaped then its termination was either due to a process
// exit or a signal like SIGTERM or SIGKILL.
// If something failed during spawning or collection of
// exit codes, then the ExitType will be set to Internal and
// an integer code will explain what the problem was.
enum class ExitType 
{
    Exit,          // exit via exit(),_exit() or returning from main()
    Signal,        // terminated by a signal
    Internal       // other condition, see codes below
};

class ExitStatus 
{
public:

    ExitStatus() : exitType(ExitType::Internal),status(UNKNOWN) {}

    ExitStatus(ExitType aType,unsigned aStatus) : 
        exitType(aType),status(aStatus) {}


    ExitType exitType;

    // interpretation depends on exitType:
    // - exitType=Exit:     status is the process exit code
    // - exitType=Signal:   status is the signal number causing termination
    // - exitType=Internal: status is one of the ints listed below
    int status;

    // shell wrappers may convert signals to exits with code > 128
    // call this to convert exit status to a signal in these cases
    bool convertHighExitToSignal();

    // The following are the codes placed in status when
    // exitType is Internal

    // - Process hasn't exited yet
    static const int NO_EXIT = 0;

    // - Process object was deleted, and therefore detached from the OS process (which
    // may not have actually exited).
    static const int PROCESS_DELETED = 1;
 
    // - fork() failed during spawn(), and therefore process was moved to Terminated
    // without ever running
    static const int FORK_FAILED = 2;

    // - terminate() was called from NotSpawned state, and so process did not run
    static const int NOT_SPAWNED = 3;

    // - Tried to terminate OS process, but it was uninterruptable, 
    // so we have detached from it with no exit code
    static const int UNINTERRUPTABLE = 4;

    // cause of exit cannot be diagnosed
    static const int UNKNOWN = 5;

    // return a string describing one of the internal codes above
    static std::string internalCodeString(int code);
};

class ProcessObserver
{
public:
    virtual ~ProcessObserver() {}
    // process mutex may be locked when these is called :
    // don't try to call functions that require the mutex!
    virtual void onTerminate(const api::UUID& id,
                             const api::UUID& sessionId,
                             ExitStatus status)=0;
    virtual void onSpawn(const api::UUID& id,
                         const api::UUID& sessionId,
                         pid_t pid)=0;
};

//--------------------------------------------------------------------------------
// When in state Spawned, represents an OS process
// In state NotSpawned, a possible future OS process
// In state Terminated, a possible past OS process
class Process
{
public:
    using Ptr = std::shared_ptr<Process>;
  
    // id and sessionId must be non-null,
    // name must not be empty, and
    // manager must remain valid for the 
    // lifetime of this object
    Process(const api::UUID& id,
            const std::string& name,
            const api::UUID& sessionId,
            ProcessManager& manager);
    ~Process();

    // immutable members
    const api::UUID& id() const { return mId; }
    const std::string& name() const { return mName; }
    const api::UUID& sessionId() const { return mSessionId; }

    // fetch state (mutex locked)
    ProcessState state();
    pid_t pid();
    ExitStatus status();
    void state(ProcessState& state, pid_t& pid, ExitStatus& status);
    

    // spawn the process if it isn't already spawned
    // fails if the process is Terminating or Terminated
    StateChange spawn(const SpawnArgs& args);

    // terminate the process if it is spawned. Can call from
    // any state : eventually process will enter Terminated
    // fast=true : go straight to kill -9, skipping nicer
    //             methods
    StateChange terminate(bool fast);

    // send a signal to the process
    void signal(int signal,bool sendToGroup);

    // move from Terminated back to NotSpawned, so
    // that process may be respawned. Fails if
    // process is not in Terminated state
    StateChange reset();

    // blocking call, returns when process has entered 
    // Terminated state. Second and third forms return 
    // false if timeout expires before process exit.
    // status can be nullptr; if not, exit status is returned
    // in *status
    void waitForExit(ExitStatus* status /* out */);
    bool waitForExit(ExitStatus* status, 
                     const std::chrono::milliseconds& timeout);
    bool waitUntilExit(ExitStatus* status, 
                       const  std::chrono::steady_clock::time_point& endtime);

private:

    friend class ProcessManager;

    void doExec(const SpawnArgs& args);

    // called to mark the process as terminated
    void terminated(const ExitStatus& status);
    void terminated_internal(int internalCode);

    // waitForExit using an already held lock
    void waitForExit_wlock(std::unique_lock<std::mutex>& lock);
    bool waitForExit_wlock(std::unique_lock<std::mutex>& lock,
                           const std::chrono::milliseconds& timeout);
    bool waitUntilExit_wlock(std::unique_lock<std::mutex>& lock,
                             const  std::chrono::steady_clock::time_point& endtime);

    // runs in async termination thread
    void terminationProc(bool fast); 
  
    bool atLeast(ProcessState ps) { return (unsigned)mState >= (unsigned)ps; }
    std::string logname() { return mName + " (" + mId.toString() + ")"; }

    const api::UUID mId;               // unique identifier
    const std::string mName;
    const api::UUID mSessionId;        // group identifier (used for logging)
    ProcessManager& mManager;
    bool mCleanupProcessGroup = false;          // set from SpawnArgs
    std::shared_ptr<ProcessObserver> mObserver; //       "
    
    // resource management
    bool mEnforceMemory = false;
    unsigned mAssignedMb = 0;
    bool mEnforceCores = 0;
    unsigned mAssignedCores = 0;

    std::thread mTerminationThread;
    
    // state variables protected by a mutex
    mutable std::mutex mStateMutex;
    pid_t mPid = 0;
    ProcessState mState;
    ExitStatus mStatus;
    std::condition_variable mExitCondition;
    bool mCGroupExists = false;
    unsigned mReservedMb = 0;
    unsigned mBorrowedMb = 0;
  
};


}
}
#endif

