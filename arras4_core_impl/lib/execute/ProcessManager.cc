// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ProcessManager.h"
#include "process_utils.h"
#include "ControlGroup.h"
#include "ProcessController.h"
#include "process_utils.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <unistd.h>
#include <signal.h> 
#include <sys/wait.h>

namespace {

unsigned long constexpr ONE_MB = 1024L*1024L;

// sleep between calls to waitpid, to check for exited processes
useconds_t constexpr EXIT_CHECK_INTERVAL_USEC = 100000; // 0.1 seconds

// sleep between checks to see whether all children of a process are dead
useconds_t constexpr CHILD_CLEANUP_CHECK_INTERVAL_USEC = 500000; // 0.5 seconds

// sleep between checks to see if borrowed memory has been released
useconds_t constexpr BORROW_CHECK_INTERVAL_USEC = 10000; // 0.01 seconds

// wait timeout for control group oom check
int constexpr OOM_WAIT_INTERVAL_MSEC = 1000;

// This proc runs in a detached background thread to make sure all
// processes in the group are killed.
// It is run when a process with the "cleanupProcessGroup" flag set
// terminates
void groupCleanupProc(pid_t group,
                      const ::arras4::api::UUID& parentId,
                      const std::string& parentName,
                      const ::arras4::api::UUID& parentSession)
{ 
    ARRAS_DEBUG("Cleaning up process group for " << parentName << " (" <<
	       parentId.toString() << ") pid " << group);

    arras4::log::Logger::instance().setThreadName("process group cleanup");

    bool hasMembers = ::arras4::impl::doesProcessGroupHaveMembers(group);
    if (hasMembers) {
        int tries=0; 
        do {
            usleep(CHILD_CLEANUP_CHECK_INTERVAL_USEC);
            hasMembers = ::arras4::impl::doesProcessGroupHaveMembers(group);
            tries++;
        } while (hasMembers && (tries < 10));

        // if there are still members of the process group 
        // alive then do a sigkill of the process group
        if (hasMembers) {
            ARRAS_WARN(::arras4::log::Id("warnProcGroupSurvived") <<
                        ::arras4::log::Session(parentSession.toString()) <<
                        "Process group for " << parentName << " (" << 
                        parentId.toString() << ") not gone. Sending SIGKILL");
                    // sending a negative process id indicates to signal the 
                    // entire process group rather than just the process
            kill(-group, SIGKILL);
                 
        }
    }
}

} // anon namespace

namespace arras4 {
    namespace impl {

ProcessManager::ProcessManager(unsigned availableMemoryMb, // = 0
                               bool useCgroups,    // = false
                               bool enforceMemory, // = true,
                               bool enforceCpu,    //  = true,
                               bool loanMemory)    // = false
    :  mRunThreads(true),
       mUseControlGroups(useCgroups),
       mEnforceMemory(enforceMemory), 
       mLoanMemory(loanMemory),
       mEnforceCpu(enforceCpu)
{
    mMemory.set(availableMemoryMb);
    mExitMonitorThread = std::thread(&ProcessManager::exitMonitorProc,this);
    initControlGroups(); 
    if (mUseControlGroups)
        mOomMonitorThread = std::thread(&ProcessManager::oomMonitorProc,this);
}

void 
ProcessManager::setProcessController(std::shared_ptr<ProcessController> controller)
{
    mProcessController = controller;
}

ProcessManager::~ProcessManager()
{
    mRunThreads = false;
    if (mExitMonitorThread.joinable())
        mExitMonitorThread.join();
    if (mOomMonitorThread.joinable())
        mOomMonitorThread.join();
}
    
Process::Ptr ProcessManager::addProcess(const api::UUID& id,
                                      const std::string& name,
                                      const api::UUID& sessionId)
{
    std::string n(name);
    if (n.empty())
        n = "Process_" + id.toString();
    api::UUID sid(sessionId);
    if (sid.isNull())
        sid = id;
    Process::Ptr pp = std::make_shared<Process>(id,n,sid,*this);
    std::unique_lock<std::mutex> lock(mProcessesMutex);
    mProcesses[id] = pp;
    return pp;
}

Process::Ptr ProcessManager::getProcess(const api::UUID& id)
{
    if (id.isNull()) return Process::Ptr();
    std::lock_guard<std::mutex> lock(mProcessesMutex);
    return getProcess_wlock(id);
}

Process::Ptr ProcessManager::getProcess_wlock(const api::UUID& id)
{
    std::map<api::UUID,Process::Ptr>::iterator it = mProcesses.find(id);
    if (it == mProcesses.end()) {
        return Process::Ptr();
    }
    return it->second;
}

bool ProcessManager::removeProcess(const api::UUID& id)
{
    if (id.isNull()) return false;
    Process::Ptr pp;
    {
        std::lock_guard<std::mutex> lock(mProcessesMutex);
        std::map<api::UUID,Process::Ptr>::iterator it = mProcesses.find(id);
        if (it == mProcesses.end()) {
            return false;
        }
        pp = it->second;
    }
    StateChange sc = pp->terminate(true);
    if (!StateChange_success(sc)) {
        return false;
    }
    pp->waitForExit(nullptr);
    {
        std::lock_guard<std::mutex> lock(mProcessesMutex);
        mProcesses.erase(id);
    }
    return true;
    
}
// called from main process just before forking (during spawn)
void  ProcessManager::preFork_cb(Process& p)
{
    reserveMemory(p);
    if (mControlGroup) {
        createControlSubgroup(p);
    }       
}
    
// called from main process after fork has failed (during spawn)
void  ProcessManager::failedFork_cb(Process& p)
{
    releaseMemory(p);
    if (mControlGroup) {
        destroyControlSubgroup(p);
    }
}
    
// called from main process after fork has succeeded (during spawn)
void  ProcessManager::postFork_cb(Process& p)
{
    // add p to the pid->process map, so we can look it up when
    // the process exits
    std::lock_guard<std::mutex> lock(mProcessesMutex);
    pid_t pid = p.mPid;  // don't use .pid() : the lock is already held
    Process::Ptr pp = getProcess_wlock(p.id());
    if (pp && pid) {
        mPidToProcess[pid] = pp;
    }
}

// called from child process after fork has succeeded (during spawn)
void ProcessManager::postForkChild_cb(Process& p)
{  
    // add child process to control subgroup so
    // that its resource use will be monitored by OS
    if (mControlGroup) {
        addChildToSubgroup(p);
    }
}

// called from exitMonitorProc when a spawned process exits
void ProcessManager::exit_cb(Process& p)
{
    releaseMemory(p);
    if (mControlGroup) {
        destroyControlSubgroup(p);
    }
}

// called to attempt to terminate a process by asking
// it politely to stop. The process state mutex
// is held when this is called
bool ProcessManager::controlledStop(Process& p)
{
    if (mProcessController)
        return mProcessController->sendStop(p.id(),p.sessionId());
    return false;
}

// runs in a thread to monitor for child processes that
// exit. 
void ProcessManager::exitMonitorProc()
{
    log::Logger::instance().setThreadName("process exit monitor");

    std::vector<pid_t> pidList;
    while (mRunThreads) {

	// collect a list of pids to check
	{
	    pidList.clear();
	    std::lock_guard<std::mutex> lock(mProcessesMutex);
	    for (const auto& entry : mPidToProcess) {
		pidList.push_back(entry.first);
	    }
	}

	// check every pid on the list
	int status;
	for (pid_t aPid : pidList) {
	    if (waitpid(aPid,&status,WNOHANG) > 0) {
		handleChildExit(aPid,status);
	    }
	}

        // wait before next check
        usleep(EXIT_CHECK_INTERVAL_USEC);
    }
}

// handle the exit of a child process
void ProcessManager::handleChildExit(pid_t pid,int status)
{
     std::lock_guard<std::mutex> lock(mProcessesMutex);
     std::map<pid_t,Process::Ptr>::iterator it = mPidToProcess.find(pid);
     if (it == mPidToProcess.end()) {
         ARRAS_WARN(log::Id("unmanagedProcessExit") <<
                    "Collected exit of unmanaged process " << pid);
         return;
     }
 
     Process::Ptr pp = it->second;

     exit_cb(*pp);

     // remove pid from the map
     mPidToProcess.erase(pid);

     // determine exit status
     ExitStatus exitStatus;
     if (WIFEXITED(status)) {
         exitStatus.exitType = ExitType::Exit;
         exitStatus.status =  WEXITSTATUS(status);
     } else if (WIFSIGNALED(status)) {
         exitStatus.exitType = ExitType::Signal;
         exitStatus.status = WTERMSIG(status);
     }
     
     // mark p as terminated
     pp->terminated(exitStatus);

     // if p has "cleanupProcessGroup" flag, run an async detached step
     // to kill all processes in the group
     if (pp->mCleanupProcessGroup) {
         // set up the thread, passing in everything it needs so that it
         // doesn't need to rely on *pp's continued existence
         std::thread cleanupThread(groupCleanupProc,pid,pp->id(),pp->name(),pp->sessionId());
         cleanupThread.detach();
     }
}

//--------------------------------------------------------------------
// Resource tracking : cgroups

std::string subgroupName(Process& p) 
{
    return p.id().toString();
}

Process::Ptr ProcessManager::sgNameToProcess(const std::string& name)
{
    return getProcess(api::UUID(name));
}

void ProcessManager::initControlGroups()
{
    if (mUseControlGroups) {
        try {
            mControlGroup.reset(new ControlGroup());
            mControlGroup->setBaseGroup("arras");
        } catch (const std::runtime_error& e) {
            ARRAS_ERROR(log::Id("errorInitializingCgroups") << 
                        "Can't initialize cgroups: " << e.what());
            mControlGroup.reset();
            if (mEnforceMemory || mEnforceCpu) {
                ARRAS_ERROR(log::Id("cgroupsRequired") << 
                            "cgroups are required to implement memory and cpu controls: " << 
                            e.what());
            }
            mUseControlGroups = mEnforceMemory = mEnforceCpu = mLoanMemory = false;
        }
        ARRAS_DEBUG("Control groups enabled");
        ARRAS_DEBUG("Memory limit enforcement " <<  (mEnforceMemory ? "enabled" : "disabled"));
        if (mEnforceMemory) ARRAS_DEBUG("Memory loaning " << (mLoanMemory ? "enabled" : "disabled"));
        ARRAS_DEBUG("Cpu limit enforcement " << (mEnforceCpu ? "enabled" : "disabled"));
        ARRAS_DEBUG("Control groups " << (mEnforceCpu ? "enabled" : "disabled"));

        
    } else {
        // clear these so that true can imply mUseControlGroups is true
        mEnforceMemory = mEnforceCpu = mLoanMemory = false;
        ARRAS_DEBUG("Control groups disabled");
    }
}

// note p's state mutex is locked when this is called : access members directly
// rather than using the thread-safe accessors
void ProcessManager:: createControlSubgroup(Process& p)
{
    if (p.mCGroupExists) return;

    std::string sgName = subgroupName(p);
    unsigned long bytes = 0;
    if (p.mEnforceMemory) {
        bytes = static_cast<unsigned long>(p.mAssignedMb) * ONE_MB;
    }
    float localCores = -1.0;
    if (mEnforceCpu) {
        localCores = static_cast<float>(p.mAssignedCores);
    }
    
    try {
        mControlGroup->createSubgroup(sgName, bytes, bytes, localCores);
        p.mCGroupExists = true;
    } catch (const std::runtime_error& err) {
        ARRAS_ERROR(log::Id("createCGroupFailed") <<
                    log::Session(p.sessionId().toString()) <<
                    "Error creating cgroup " << sgName << " : " << err.what());
        // TODO this should be fatal once we're enforcing things. It is probably a misconfigured
        // cgroups environment.
    }
}


void ProcessManager::addChildToSubgroup(Process& p)
{
    if (!p.mCGroupExists) return;

    const std::string sgName = subgroupName(p);
    try {
       mControlGroup->addSelfSubgroup(sgName);
    } catch (const std::runtime_error& err) {
        ARRAS_ERROR(log::Id("addToCGroupFailed") <<
                    log::Session(p.sessionId().toString()) <<
                    "Error adding process to cgroup " << sgName << " : " << err.what());
    }
}

void ProcessManager::destroyControlSubgroup(Process& p)
{
    if (!p.mCGroupExists) return;

    const std::string sgName = subgroupName(p);
    try {
        mControlGroup->destroySubgroup(sgName);
        p.mCGroupExists = false;
    } catch (const std::runtime_error& err) {
        ARRAS_ERROR(log::Id("destroyCGroupFailed") <<
                    log::Session(p.sessionId().toString()) <<
                    "Error destroying cgroup " << sgName << " : " << err.what());
    }
}

// runs in a thread to monitor for child processes that
// generate an out-of-memory condition 
void ProcessManager::oomMonitorProc()
{
    log::Logger::instance().setThreadName("process out-of-memory monitor");
    std::vector<std::string> groups;
    while (mRunThreads) {
        if (mControlGroup->waitOomStatusSubgroup(groups, OOM_WAIT_INTERVAL_MSEC)) {
            handleOomCondition(groups);
        }
    }
}

// handle out-of-memory conditions. groups
// is a vector of control subgroups
void ProcessManager::handleOomCondition(const std::vector<std::string>& groups)
{
    for (size_t i=0; i < groups.size(); i++) {
        std::string group = groups[i];
        ARRAS_INFO(log::Id("computationOutOfMemory") << 
                   "Group " << group << "is out of memory");
       
        Process::Ptr pp = sgNameToProcess(group);;
        if (!pp) {
            ARRAS_ERROR(log::Id("invalidCgroupName") <<
                        "cgroup name " << group << " is not in the expected format : it should be a process id");
            return;
        }
         
        if (pp->state() != ProcessState::Spawned) {
            ARRAS_ERROR(log::Id("outOfMemoryWhileTerminating") << 
                        log::Session(pp->sessionId().toString()) <<
                        "Process " << pp->logname() << " ran out of memory while terminating or terminated.");
            return;
        }

        if (mLoanMemory) {
            unsigned int borrowed = mMemory.borrow(128);
            if (borrowed > 0) {
                std::unique_lock<std::mutex> lock(pp->mStateMutex);
                pp->mBorrowedMb += borrowed;
                unsigned totalMb = pp->mBorrowedMb + pp->mReservedMb;
                unsigned long total = static_cast<unsigned long>(totalMb) * 1048576;
                const std::string sgName = subgroupName(*pp);
                mControlGroup->changeMemoryLimitSubgroup(sgName, total, total);
                ARRAS_INFO(log::Id("lentMemory") << 
                           log::Session(pp->sessionId().toString()) <<
                           "Lent " << borrowed << "MB of memory to " <<
                           pp->logname() << " (" << pp->mBorrowedMb << 
                           "MB total lent) for total limit of " << totalMb);
            } else {
                ARRAS_ERROR(log::Id("noMemoryToLend") <<
                            log::Session(pp->sessionId().toString()) <<
                            "No more memory available to lend to " << 
                            pp->logname() << ". Killing the process.");
                // a process in the out of memory state will only die with a SIGKILL
                pp->terminate(true);
                // stop monitoring the memory so it won't show up on future monitoring if it is slow to die
                mControlGroup->monitorOomSubgroup(group, false);
            }
        } else {
            ARRAS_ERROR(log::Id("exceededMemoryLimit") << 
                        log::Session(pp->sessionId().toString()) <<
                        "Killing " << pp->logname() << "for exceeding memory limit");
            // a process in the out of memory state will only die with a SIGKILL
            pp->terminate(true);
            // stop monitoring the memory so it won't show up on future monitoring if it is slow to die
            mControlGroup->monitorOomSubgroup(group, false);
        }
    }
}

//------------------------------------------------------------------------
// Resource tracking : memory reservation
// note p's state mutex is locked when this is called : access members directly
// rather than using the thread-safe accessors
void ProcessManager::reserveMemory(Process& p)
{
    unsigned reservation = p.mAssignedMb;
    long deficit = mMemory.reserve(reservation);
    if (mLoanMemory) {
        // The needed memory may be in use by a computation which has exceeded its formal allocation
        // In that case we need to kill the borrowers. We kill the biggest borrowers first since
        // that privides the biggest benefit and they are the biggest violators anyay.
        if (deficit > 0) {
            // kill enough borrower sessions to deal with the deficit
            while (deficit > 0) {
                unsigned int borrowedAmount;
                Process::Ptr borrowerPtr = findBiggestBorrower(borrowedAmount);
                if (!borrowerPtr) break;
                borrowerPtr->terminate(true);
                deficit -= borrowedAmount;

                // wait for the borrowing processes to be reaped
                while (borrowerPtr->mBorrowedMb > 0) {
                    usleep(BORROW_CHECK_INTERVAL_USEC);
                }
            }

            // wait for them to be reaped and have the memory available before spawning
            // the process that needs the memory
            deficit = mMemory.reserve(reservation);
            if (deficit > 0) {
                ARRAS_ERROR(log::Id("InsufficientMemory") <<
                            log::Session(p.sessionId().toString()) <<
                            "Unexpected deficit after borrowers were reaped");
                reservation -= (int)deficit;
                mMemory.reserve(reservation);
            }
        }
    } else if (deficit > 0) {
        reservation -= (int)deficit; 
        mMemory.reserve(reservation);
    }
    p.mReservedMb = reservation;
}

// note p's state mutex is locked when this is called : access members directly
// rather than using the thread-safe accessors
void ProcessManager::releaseMemory(Process& p)
{
    mMemory.release(p.mReservedMb);
    p.mReservedMb = 0;
}

// find the process borrowing the most memory. Not
// optimized for performance...
Process::Ptr ProcessManager::findBiggestBorrower(unsigned& borrowedAmount)
{
    unsigned maxBorrowed = 0 ;
    Process::Ptr borrower;
    std::lock_guard<std::mutex> lock(mProcessesMutex);
    for (auto const& procIt : mProcesses) {
        unsigned borrowed = procIt.second->mBorrowedMb;
        if (borrowed > maxBorrowed) {
            maxBorrowed = borrowed;
            borrower = procIt.second;
        }
    }
    borrowedAmount = maxBorrowed;
    return borrower;
}

}
}
