// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Process.h"
#include "ProcessManager.h"
#include "IoCapture.h"
#include "SpawnArgs.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <unistd.h>
#include <string.h> //strerror_r
#include <sys/stat.h>
#include <signal.h>


namespace {

// exit status returned by process when execv fails. This is chosen for
// compatibility with existing Arras code
int EXITSTATUS_EXECV_FAIL = 5;

// delays used in async termination
// - how long to wait for a "stop" command to terminate process
const std::chrono::milliseconds STOP_WAIT(5000);
// - how long to wait for a SIGTERM to terminate process
const std::chrono::milliseconds SIGTERM_WAIT(5000);
// - how long to wait for a SIGKILL to terminate process
const std::chrono::milliseconds SIGKILL_WAIT(5000);


// thread proc to capture io from a process
void ioCaptureProc(std::shared_ptr<::arras4::impl::IoCapture> capture,
                   int fdStdout, int fdStderr)
{
    ::arras4::log::Logger::instance().setThreadName("IO capture thread");

    char buf[1024] = {0};
    ssize_t cs = 1, ce = 1;
    while (cs > 0 || ce > 0) {
	if (cs > 0) cs = read(fdStdout, buf, 1024);
        if (cs > 0) capture->onStdout(buf,static_cast<unsigned>(cs));
	if (ce > 0) ce = read(fdStderr, buf, 1024);
	if (ce > 0) capture->onStderr(buf,static_cast<unsigned>(ce));
    }   
   
    close(fdStdout);
    close(fdStderr);
}

}

namespace arras4 {
    namespace impl {

Process::Process(const api::UUID& id,
                 const std::string& name,
                 const api::UUID& sessionId,
                 ProcessManager& manager) :
    mId(id), mName(name), mSessionId(sessionId), 
    mManager(manager),
    mState(ProcessState::NotSpawned),
    mStatus(ExitType::Internal,ExitStatus::NO_EXIT)
{
    if (id.isNull() || sessionId.isNull())
        throw std::invalid_argument("'Process' requires a valid id and sessionId");
    if (name.empty())
        throw std::invalid_argument("'Process' requires a non-empty name");
}

Process::~Process()
{
    terminated(ExitStatus(ExitType::Internal,ExitStatus::PROCESS_DELETED));
    if (mTerminationThread.joinable())
        mTerminationThread.join();
}
 
pid_t Process::pid()
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    return mPid;
}

ProcessState Process::state()
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    return mState;
}

ExitStatus Process::status()
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    return mStatus;
}

void Process::state(ProcessState& state, pid_t& pid, ExitStatus& status)
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    state = mState;
    pid = mPid;
    status = mStatus;
}

// spawn process : move process to Spawned state
// when in NotSpawned state, spawns process and moves to Spawned
//       (errors in spawning move state to Terminated)
// when in Spawned, returns immediately (Achieved)
// when in Terminating or Terminated, returns immediately (Invalid)
// * exit condition is that state cannot be NotSpawned
StateChange Process::spawn(const SpawnArgs& args) 
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    if (atLeast(ProcessState::Terminating))
        return StateChange::Invalid;
    if (atLeast(ProcessState::Spawned))
        return StateChange::Achieved;
    
    mEnforceMemory = args.enforceMemory;
    mEnforceCores = args.enforceCores;
    mAssignedMb = args.assignedMb;
    mAssignedCores = args.assignedCores;
    mObserver = args.observer;
    mCleanupProcessGroup = args.cleanupProcessGroup;

    // create pipes for capturing stdout,stderr
    int fdStdout[2];
    int fdStderr[2];
    if (args.ioCapture) {
        int err = 0;
        if (pipe(fdStdout) == -1) { 
            err = errno;
        } else if (pipe(fdStderr) == -1) {
            err = errno;
        }
        if (err) {
            char buf[1024];
            ARRAS_ERROR(log::Session(mSessionId.toString()) <<
                        log::Id("pipeFailed") <<
                        "Failed to create pipe for " << logname() << " : " <<
                        std::string(strerror_r(err, buf, 1024)));
            terminated_internal(ExitStatus::FORK_FAILED);
            return StateChange::Terminated;
        }
    }

    ARRAS_DEBUG(log::Session(mSessionId.toString()) <<
               "Spawning: " << args.debugString(0,true));

    // in NotSpawned state, move to Spawned or Terminated
    // fork the process
    mManager.preFork_cb(*this);
    pid_t pid = fork();
    if (pid == -1) {
        // failed fork, go to Terminated
        char buf[1024];
        ARRAS_ERROR(log::Session(mSessionId.toString()) <<
                    log::Id("forkFailed") <<
                    "Failed to fork " << logname() << " : " <<
                    std::string(strerror_r(errno, buf, 1024)));
        terminated_internal(ExitStatus::FORK_FAILED);
        mManager.failedFork_cb(*this);
        return StateChange::Terminated;
    }
    if (pid != 0) {
        // parent process, successful fork
        mPid = pid;
        mState = ProcessState::Spawned;
        mManager.postFork_cb(*this);
        
        // start io capture thread
        if (args.ioCapture) {
            close(fdStdout[1]);
            close(fdStderr[1]);
            std::thread captureThread(&ioCaptureProc,args.ioCapture,fdStdout[0],fdStderr[0]);
            captureThread.detach();
        }

        if (mObserver) mObserver->onSpawn(mId,mSessionId,pid);

        ARRAS_DEBUG(log::Session(mSessionId.toString()) <<
                   "Spawned: " << args.program << " PID: " << pid);

        return StateChange::Success;
    }

    // child process
    mManager.postForkChild_cb(*this);

    // redirect stdout, stderr to pipes 
    // that we created before fork
    if (args.ioCapture) {
        int err = 0;
        close(fdStdout[0]);
        close(fdStderr[0]);
        close(STDOUT_FILENO);
        if (dup2(fdStdout[1], STDOUT_FILENO) == -1) {
            err = errno;
        } else {
            close(STDERR_FILENO);
            if (dup2(fdStderr[1], STDERR_FILENO) == -1) {
                err = errno;
            }
        }
        if (err) {
            char buf[1024];
            ARRAS_ERROR(log::Session(mSessionId.toString()) <<
                        log::Id("ioRedirectFailed") <<
                        "Failed to redirect stdout/err for " << logname() << " : " <<
                        std::string(strerror_r(errno, buf, 1024)));
            _exit(EXITSTATUS_EXECV_FAIL);
        }
    } 
    
    // cannot return
    doExec(args);
       
    return StateChange::Invalid; // otherwise compiler might complain...
}
  
// terminate process : move process to Terminating or Terminated
// when in NotSpawned state, moves straight to Terminated without passing go
// when in Spawned, moves to Terminating and starts a termination thread
// when in Terminating returns straight away (InProgress)
// when in Terminated, return straight away (Achieved)
// * exit condition : process is in state Terminating or Terminated.
// process will always eventually achieve Terminated
// if 'fast' is true, the termination thread will run a faster path that
// doesn't try to terminate the process in a nice way.
StateChange Process::terminate(bool fast = false)
{
    {
        std::unique_lock<std::mutex> lock(mStateMutex);
        if (mState == ProcessState::NotSpawned) {
            terminated_internal(ExitStatus::NOT_SPAWNED);
            return StateChange::Success;
        }
        if (mState == ProcessState::Terminating) {
            return StateChange::InProgress;
        }
        if (mState == ProcessState::Terminated) {
             return StateChange::Success;
        }

        // state is Spawned
        mState = ProcessState::Terminating;
    }

    // set off a termination thread
    mTerminationThread = std::thread(&Process::terminationProc,this,fast);
    return StateChange::InProgress;
}

void
Process::signal(int signum, bool sendToGroup)
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    if (mPid == 0) return;
    if (sendToGroup) kill(-mPid,signum);
    else kill(mPid,signum);
}

// move from Terminated back to NotSpawned, ready to spawn again
// only valid in Terminated state
StateChange Process::reset()
{
    {
        std::unique_lock<std::mutex> lock(mStateMutex);
        if (mState != ProcessState::Terminated) {
            return StateChange::Invalid;
        }
        // no-one else can exit Terminated, so it
        // is safe to release the lock here
    }
    // make sure any existing termination thread is finished
    if (mTerminationThread.joinable())
        mTerminationThread.join();

    std::unique_lock<std::mutex> lock(mStateMutex);
    mState = ProcessState::NotSpawned;
    return StateChange::Success;
}

// wait for the process to enter 'Terminated'
void Process::waitForExit(ExitStatus* status)
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    waitForExit_wlock(lock); 
    if (status) *status = mStatus;
}

// returns false if the process doesn't exit before timeout expires
bool Process::waitForExit(ExitStatus* status,
                          const std::chrono::milliseconds& timeout)
{
    return waitUntilExit(status,std::chrono::steady_clock::now() + timeout);
}

// like waitForExit, but timeout is an absolute time.
bool Process::waitUntilExit(ExitStatus* status,
                            const  std::chrono::steady_clock::time_point& endtime)
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    if (waitUntilExit_wlock(lock,endtime)) {
        if (status) *status = mStatus;
        return true;
    }
    return false;
}

// same as waitForExit(), but caller must already have a lock and
// status is not returned
void Process::waitForExit_wlock(std::unique_lock<std::mutex>& lock)
{
    while (mState != ProcessState::Terminated) {
        mExitCondition.wait(lock);
    }
}

bool Process::waitForExit_wlock(std::unique_lock<std::mutex>& lock,
                                const std::chrono::milliseconds& timeout)
{
    return waitUntilExit_wlock(lock,std::chrono::steady_clock::now() + timeout);
}   
 
bool Process::waitUntilExit_wlock(std::unique_lock<std::mutex>& lock,
                                  const  std::chrono::steady_clock::time_point& endtime)
{
    while (mState != ProcessState::Terminated) {
        std::cv_status cvs = mExitCondition.wait_until(lock,endtime);
        if (cvs == std::cv_status::timeout)
            return false;
    }
    return true;
}    

// called to mark the process as terminated because
// of termination of an external OS process
void Process::terminated(const ExitStatus& status)
{
    {
        std::unique_lock<std::mutex> lock(mStateMutex);
        if (mState != ProcessState::Terminated) {
            mStatus = status;
            mManager.exit_cb(*this);
            mState = ProcessState::Terminated;
            mPid = 0;
        }
    }
    mExitCondition.notify_all();
    if (mObserver) mObserver->onTerminate(mId,mSessionId,mStatus);
}

// called to mark the process as terminated
// when an OS process didn't terminate. 
// NOTE: requires a state lock to be held by caller
void Process::terminated_internal(int internalStatus)
{
    if (mState != ProcessState::Terminated) {
        mStatus = ExitStatus(ExitType::Internal,internalStatus);
        mState = ProcessState::Terminated;
        mPid = 0;
    }
    mExitCondition.notify_all();
    if (mObserver) mObserver->onTerminate(mId,mSessionId,mStatus);
}

// called in child to exec the process.
// function cannot return
void Process::doExec(const SpawnArgs& args)
{
    // make this process the top of a new process group
    // so we can deliver signals to everything for shutdown
    setpgid(getpid(), getpid());

    // set the working directory, if non-empty
    if (!args.workingDirectory.empty()) {
        struct stat s;
        if (stat(args.workingDirectory.c_str(), &s) == 0) {
            if (s.st_mode & S_IFDIR) {
                if (chdir(args.workingDirectory.c_str()) !=0) {
                    char buf[1024];
                    ARRAS_WARN(log::Session(mSessionId.toString()) <<
                               log::Id("warnWorkingingDirectory") <<
                               "Could not chdir to working directory '" << 
                               args.workingDirectory <<
                               "' for " << logname() << " : " << 
                               std::string(strerror_r(errno, buf, 1024)));
                }
            } else {
                ARRAS_WARN(log::Session(mSessionId.toString()) <<
                           log::Id("warnWorkingingDirectory") <<
                           "Working directory: '" << args.workingDirectory <<
                           "' does not exist, for " <<  logname());
            }
        } else {
            char buf[1024];
            ARRAS_WARN(log::Session(mSessionId.toString()) <<
                       log::Id("warnWorkingingDirectory") <<
                       "Could not stat working directory: '" << 
                       args.workingDirectory <<
                       "' for " << logname() << " : " << 
                       std::string(strerror_r(errno, buf, 1024))); 
        }
    }

    // convert args to c format
    char* cargs[args.args.size()+2];
    cargs[0] = const_cast<char*>(args.program.c_str());
    for (size_t i = 1; i < args.args.size()+1; ++i) {
        cargs[i] = const_cast<char*>(args.args[i-1].c_str());
    }
    cargs[args.args.size()+1] = nullptr;
    
    // convert env to c format
    std::vector<std::string> envVec = args.environment.asVector();
    char* cenv[envVec.size()+1];
    for (size_t i = 0; i < envVec.size(); ++i) {
        cenv[i] = const_cast<char*>(envVec[i].c_str());
    }
    cenv[envVec.size()] = nullptr;

    // and exec program
    execvpe(args.program.c_str(),cargs,cenv);
    
    // returning from exec is a fatal error, exit
    // the child process
    char buf[1024];
    ARRAS_FATAL(log::Session(mSessionId.toString()) <<
                log::Id("execFailed") <<
                "Failed to exec " << logname() << " : " <<
                std::string(strerror_r(errno, buf, 1024)));

    // ARRAS-3227 : running exit() crashes the parent process, 
    // replaced with _exit()
    _exit(EXITSTATUS_EXECV_FAIL); 
}

// this function runs asynchronously to terminate
// a process. On entry, the state should be "Terminating",
// on exit state will be "Terminated" provided entry condition is met
void Process::terminationProc(bool fast)
{ 
    log::Logger::instance().setThreadName(mName + " termination thread");
    std::unique_lock<std::mutex> lock(mStateMutex);
    if (mState != ProcessState::Terminating ||
        mPid == 0) {
        return;
    }

    if (!fast) {

        // first manager gets a chance to "ask" the process
        // to stop
        if (mManager.controlledStop(*this))
        {
            if (waitForExit_wlock(lock,STOP_WAIT))
                return;
        }

        // no good, so send SIGTERM to the process group
        kill(-mPid,SIGTERM);
        if (waitForExit_wlock(lock,SIGTERM_WAIT))
            return;    
        ARRAS_ERROR(log::Session(mSessionId.toString()) <<
                    log::Id("didntTerm") <<
                    "Timed out waiting for " << logname() << 
                    " to respond to SIGTERM. Sending SIGKILL.");
    }

    // send SIGKILL to the process group
    kill(-mPid,SIGKILL);
    if (waitForExit_wlock(lock,SIGKILL_WAIT))
        return;    
    ARRAS_ERROR(log::Id("didntKill") <<
                log::Session(mSessionId.toString()) << 
                "Timed out waiting for " << logname() << 
                " to respond to SIGKILL. It appears to be uninterruptable.");
    // not much we can do except mark the process as terminated and ignore it
    terminated_internal(ExitStatus::UNINTERRUPTABLE);
}



/*static*/ std::string 
ExitStatus::internalCodeString(int code)
{
    switch (code) {
    case NO_EXIT: return "has not exited";
    case PROCESS_DELETED: return ": process object was deleted";
    case FORK_FAILED: return ": fork() system call failed while attempting to launch";
    case NOT_SPAWNED: return "did not run";
    case UNINTERRUPTABLE: return "cannot be terminated";
    case UNKNOWN:
    default:
	return ": execution status is unknown";
    }
}

bool
ExitStatus::convertHighExitToSignal()
{
    if (exitType == ExitType::Exit &&
	status > 128 &&
	status < (128+32)) {
	exitType = ExitType::Signal;
	status -= 128;
	return true;
    }
    return false;
}
    
}
}
