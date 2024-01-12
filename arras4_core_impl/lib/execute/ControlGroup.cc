// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <cstring>
#include <stdexcept>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>       // for printf
#include <stdlib.h>      // for free()
#include <string>        // for std::string
#include <sys/eventfd.h> // for eventfd()
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>      // for getpid() and pid_t


#include "ControlGroup.h"

#if defined(__ICC)
#define ARRAS_START_THREADSAFE_STATIC_WRITE       __pragma(warning(disable:1711))
#define ARRAS_FINISH_THREADSAFE_STATIC_WRITE __pragma(warning(default:1711))
#else
#define ARRAS_START_THREADSAFE_STATIC_WRITE
#define ARRAS_FINISH_THREADSAFE_STATIC_WRITE
#endif

namespace arras4 {
namespace impl {

std::mutex ControlGroup::mInitializeMutex;
bool ControlGroup::mInitialized = false;
const unsigned long CPU_PERIOD = 100000;

using std::runtime_error;

//
// Constructing the control group checks that there is a valid general cgroup
// environment. If there isn't a valid environment the objext still constructs
// but is in an invalid state, which can be determined by calling valid().
// The cause for being invalid get be read as a string by calling status().
//
ControlGroup::ControlGroup() :
    mEventFd(-1),
    mInitializeStatus(2),
    mFastOomCheck(false),
    mUseHierarchy(false),
    mValid(false), // assume it's invalid until proven valid
    mValidBase(false)
{
    int status = 0;

    // initialize cgroup library
    {
        std::unique_lock<std::mutex> lock(mOomFdsMutex);
        if (!mInitialized) {
            status = cgroup_init();
            if (status != 0) {
                mStatus = "Couldn't initialize libcgroups";
                return;
            }
            ARRAS_START_THREADSAFE_STATIC_WRITE
            // we're  under a lock
            mInitialized = true;
            ARRAS_FINISH_THREADSAFE_STATIC_WRITE
        }
    }

    // check that the cpu and memory controllers have mount points
    char* path = 0;
    status = cgroup_get_subsys_mount_point("cpu", &path);
    if (status != 0) {
        mStatus = std::string("Couldn't get cpu mount point: ") + cgroup_strerror(status);
    }
    free(path);

    status = cgroup_get_subsys_mount_point("memory", &path);
    if (status != 0) {
        mStatus = std::string("Couldn't get memory mount point: ") + cgroup_strerror(status);
    }
    free(path);

    // allocate a single event file descriptor which will be used to get
    // notification of out of memory events in c groups
    // CLOEVEC is so the forked processes won't have a copy of the opened fd
    mEventFd = eventfd(0, EFD_CLOEXEC);

    // everything seems to check out
    // this is thread safe because only one thread may call this function at a time
    mValid = true;
}

//
// setBaseGroup sets the prefix for cgroup names and makes sure that the
// cgroup is writeable. This must be called before any of the SubGroup
// function versions are called. Generally it is expected that will just
// be called once for the ControlGroup structure.
//
// No other threads should be calling into this instance of ControlGroup while
// setBaseGroup() is being called.
//
// If this function throws and exception then there are no changes
//
void
ControlGroup::setBaseGroup(const std::string& baseGroup)
{
    if (!mValid) throw runtime_error("Invalid ControlGroup");

    //
    // check that both cpu and memory have the base cgroup
    //
    cgroup* arrasGroup = cgroup_new_cgroup(mBaseGroup.c_str());
    if (arrasGroup == nullptr) {
        throw runtime_error("Couldn't create the group structure");
    }
    int status =  cgroup_get_cgroup(arrasGroup);
    if (status != 0) {
        freeGroup(arrasGroup);
        throw runtime_error(std::string("There was a problem getting the c group: ") +
                  mBaseGroup + ":" +
                  cgroup_strerror(status));
    }
    cgroup_controller* control = cgroup_get_controller(arrasGroup, "cpu");
    if (control == nullptr) {
        freeGroup(arrasGroup);
        throw runtime_error("Can't get the c group for cpu");
    }
    control = cgroup_get_controller(arrasGroup, "memory");
    if (control == nullptr) {
        freeGroup(arrasGroup);
        throw runtime_error("Can't get the c group for memory");
    }
   
    // 
    // Keep track of if use_hierarchy is enabled. We can't change the out of
    // memory strategy for the subgroups if use_hierarchy is enabled becaue
    // the parent controls it in that case
    // 
    unsigned long useHierarchy;
    if (cgroup_get_value_uint64(control, "memory.use_hierarchy", &useHierarchy) != 0) {
        freeGroup(arrasGroup);
        throw runtime_error("Couldn't get memory.use_hierarchy");
    }
    mUseHierarchy = (useHierarchy != 0);

    // done with the cgroup structure
    freeGroup(arrasGroup);

    // see if we can create and delete a group
    std::string groupPath = baseGroup + '/' + "test_group";
    ControlGroup::create(groupPath, 1048560, 1048560, 1.0);
    ControlGroup::destroy(groupPath);

    // Now that we're past anything that can throw. update the class
    mBaseGroup = baseGroup;
    mValidBase = true;
}

//
// Create the c group for the named group
//
void
ControlGroup::create(
    const std::string& groupPath,
    unsigned long memLimitReal,
    unsigned long memLimitRealAndSwap,
    float cpuAmount,
    bool pauseOnOom)
{
    // TODO figure out how large "huge" can be
    const unsigned long huge = 256L * 1048576 * 1048576;
    int status = 0;
    if (memLimitReal == 0) memLimitReal = huge;
    if (memLimitRealAndSwap == 0) memLimitRealAndSwap = huge;

    if (memLimitReal > huge) memLimitReal = huge;
    if (memLimitRealAndSwap > huge) memLimitRealAndSwap = huge;

    if (memLimitRealAndSwap < memLimitReal) {
        throw runtime_error("memLimitRealAndSwap must be greater than or equal to memLimitReal");
    }

    cgroup* newGroup = cgroup_new_cgroup(groupPath.c_str());
    if (newGroup == nullptr) {
        throw runtime_error("Couldn't create the group structure");
    }

    //
    // The cpu controller is used to keep the processes from using more than
    // the intended share of the cpu resources. The number of "processors" (cores
    // hyperthreads depending on system configuration) is
    // cpu.cfs_quota_us/cpu.cfs_period_us. If the quota is exceeded before the
    // period (in microseconds) finishes than all of the threads are paused until
    // the period ends. Keeping the period small keeps pause times small while
    // using a larger period might cut down context switches. 
    //
    cgroup_controller* cpuController = cgroup_add_controller(newGroup, "cpu");
    if (cpuController== nullptr) {
        throw runtime_error("Couldn't create the cpu controller");
    }
    cgroup_add_value_uint64(cpuController, "cpu.cfs_period_us", CPU_PERIOD);
    if (cpuAmount > 0.0) {
        long cpuQuota = static_cast<long>(CPU_PERIOD * cpuAmount);
        cgroup_add_value_int64(cpuController, "cpu.cfs_quota_us", cpuQuota);
    } else {
        cgroup_add_value_int64(cpuController, "cpu.cfs_quota_us", -1);
    }

    //
    // The memory controller is used to manage the amount of memory the processes
    // use.
    //     memory.limit_in_bytes
    //         controls how much physical memory is used for the program and
    //         it's disk cache.
    //                          
    //     memory.memsw.limit_in_bytes
    //         controls the total of physical memory and swap space but since
    //         our systems aren't really set up to swap we'll make this the
    //         same amount as limit_in_bytes
    //
    //     memory.oom_control
    //         indicates whether processes should be killed (0) or paused (1)
    //         when they run out of memory.
    //
    cgroup_controller* memoryController = cgroup_add_controller(newGroup, "memory");
    if (memoryController== nullptr) {
        throw runtime_error("Couldn't create the memory controller");
    }
    cgroup_add_value_uint64(memoryController, "memory.limit_in_bytes", memLimitReal);
    cgroup_add_value_uint64(memoryController, "memory.memsw.limit_in_bytes", memLimitRealAndSwap);

    if (pauseOnOom) {
        cgroup_add_value_uint64(memoryController, "memory.oom_control", 1);
    }

    // actually update the file system with the cgroup
    status = cgroup_create_cgroup(newGroup, true);
    if (status != 0) {
        throw runtime_error(std::string("Couldn't create cgroup: ") + cgroup_strerror(status));
    }
}

//
// delete the c group
//
void
ControlGroup::destroy(const std::string& groupPath)
{
    // make sure the groups are empty in both controllers
    pid_t pid = 0;
    void* handle = 0;
    int status = 0 ;
 
    // shut down oom monitoring if it is enabled
    monitorOom(groupPath, false);

    //
    // Confirm that there are no tasks still assigned to the cgroup.
    // We we use the iteration routines which we expect to declare
    // "done" on the iteration start.
    status = cgroup_get_task_begin(groupPath.c_str(), "cpu", &handle, &pid);
    if (status == 0) {
        throw runtime_error("The c group is not empty in cpu controller");
    } else if (status != ECGEOF) {
        throw runtime_error("There was an error getting task list in cpu controller");
    }
    cgroup_get_task_end(&handle);
    status = cgroup_get_task_begin(groupPath.c_str(), "memory", &handle, &pid);
    if (status == 0) {
        throw("The c group is not empty in memory controller");
    } else if (status != ECGEOF) {
        throw("There was an error getting task list in memory controller");
    }
    cgroup_get_task_end(&handle);
    

#define USE_DELETE_WORKAROUND
#ifdef USE_DELETE_WORKAROUND

    // cgroup_delete_cgroup unnecessarily opens the parent c group tasks list
    // for write on the possibility that it needs to migrate tasks to the
    // parent, requiring extra permissions. Since we will always have killed
    // all of the tasks before before destroying the c group this isn't
    // necessary. Moving the tasks to the parent will open up the memory use
    // restrictions for the task, which we never want to do.
    //
    // Since the tasks list is empty, this is just a rmdir for each controller

    char* path = 0;
    status = cgroup_get_subsys_mount_point("cpu", &path);
    if (status != 0) throw runtime_error("Can't get cpu mount point");
    std::string cpuPath = std::string(path) + "/" + groupPath;
    free(path);

    status = cgroup_get_subsys_mount_point("memory", &path);
    if (status != 0) throw runtime_error("Can't get memory mount point");
    std::string memoryPath = std::string(path) + "/" + groupPath;
    free(path);

    rmdir(cpuPath.c_str());
    rmdir(memoryPath.c_str());

#else // don't USE_DELETE_WORKAROUND

    cgroup* arrasGroup = getGroup(groupPath);

    status = cgroup_delete_cgroup(arrasGroup, true);
    freeGroup(arrasGroup);
    if (status != 0) {
        throw runtime_error(std::string("Error deleting cgroup : ") + cgroup_strerror(status));
    }

#endif // USE_DELETE_WORKAROUND
}

//
// Add a process or thread id to the c group. Existing children are not
// automatically added but new children will inherit being in the cgroup.
//
void
ControlGroup::addTask(const std::string& groupName, pid_t pid)
{
    cgroup* arrasGroup = getGroup(groupName);

    int status = cgroup_attach_task_pid(arrasGroup, pid);
    freeGroup(arrasGroup);
    if (status != 0) {
        throw runtime_error(cgroup_strerror(status));
    }
}

//
// Convenience function for adding the current process to the cgroup
//
void
ControlGroup::addSelf(const std::string& groupName)
{
    addTask(groupName, getpid());
}

//
// Update the memory limit for the cgroup
// 
// Due to limitiation of low level library code the changes could
// be partially applied if an error is thrown though it's unlikely
//
void
ControlGroup::changeMemoryLimit(
    const std::string& groupPath,
    unsigned long memLimitReal,
    unsigned long memLimitRealAndSwap)
{

    // get the existing values so we know which order to change them
    cgroup* arrasGroup= getGroup(groupPath);
    cgroup_controller* memoryController = cgroup_get_controller(arrasGroup,"memory");
    if (memoryController == nullptr) {
        freeGroup(arrasGroup);
        throw runtime_error("Error getting memory controller");
    }
    unsigned long oldRealAndSwap;
    unsigned long oldReal;
    cgroup_get_value_uint64(memoryController, "memory.memsw.limit_in_bytes", &oldRealAndSwap);
    cgroup_get_value_uint64(memoryController, "memory.limit_in_bytes", &oldReal);
    freeGroup(arrasGroup);

    // create an empty cgroup structure since we only want to update one controller
    arrasGroup = cgroup_new_cgroup(groupPath.c_str());
    if (arrasGroup == nullptr) {
        throw runtime_error("Couldn't create the group structure");
    }

    // add the memory controller
    memoryController = cgroup_add_controller(arrasGroup, "memory");
    if (memoryController== nullptr) {
        throw runtime_error("Couldn't create the memory controller");
    }

    // memory.limit_in_bytes allways has to be less than or equal to
    // memory.memsw.limit_in_bytes. When increasing memory
    // memory.memsw.limit_in_bytes has to be changed first. When decreasing
    // memory memory.limit_in_bytes has to be changed first
    if (memLimitRealAndSwap > oldRealAndSwap) {
        cgroup_add_value_uint64(memoryController, "memory.memsw.limit_in_bytes", memLimitRealAndSwap);
        cgroup_add_value_uint64(memoryController, "memory.limit_in_bytes", memLimitReal);
    } else {
        cgroup_add_value_uint64(memoryController, "memory.limit_in_bytes", memLimitReal);
        cgroup_add_value_uint64(memoryController, "memory.memsw.limit_in_bytes", memLimitRealAndSwap);
    }

    // write out the changes
    int status = cgroup_modify_cgroup(arrasGroup);
    if (status != 0) {
        std::string description = std::string("Couldn't modify cgroup :") + cgroup_strerror(status);
        freeGroup(arrasGroup);
        throw runtime_error(description);
    }

    freeGroup(arrasGroup);
}

//
// Get the actual memory usage
// If this function throws the outputs aren't written
//
// If this function throws an exception the outputs are not modified
//
void
ControlGroup::getMemoryUsage(const std::string& groupName,
                             unsigned long& memoryReal,
                             unsigned long& memoryRealAndSwap)
{
    cgroup* arrasGroup= getGroup(groupName);
    cgroup_controller* memoryController = cgroup_get_controller(arrasGroup, "memory");
    if (memoryController == nullptr) {
        freeGroup(arrasGroup);
        throw runtime_error("Couldn't get the memory controller");
    }

    unsigned long memoryUsage;
    if (cgroup_get_value_uint64(memoryController, "memory.usage_in_bytes", &memoryUsage) != 0) {
        freeGroup(arrasGroup);
        throw runtime_error("Couldn't get memory.usage_in_bytes");
    }

    unsigned long memoryAndSwapUsage;
    if (cgroup_get_value_uint64(memoryController, "memory.memsw.usage_in_bytes", &memoryAndSwapUsage) != 0) {
        freeGroup(arrasGroup);
        throw runtime_error("Couldn't get memory.memsw.usage_in_bytes");
    }

    memoryReal = memoryUsage;
    memoryRealAndSwap = memoryAndSwapUsage;
}

namespace {

//
// internal function to parse the text memory stats file into a structure
//
void
parseMemStatEntry(struct cgroup_stat& statEntry, ControlGroup::MemoryStats& stats, bool wantTotal)
{
    std::string name;
    bool isTotal = (strncmp("total_", statEntry.name, 6) == 0);
    if (isTotal && wantTotal) {
        name = std::string(statEntry.name + 6);
    } else {
        name = std::string(statEntry.name);
    }
            
    unsigned long value = std::stoul(statEntry.value);

    if (name  == "cache") {
        stats.cache = value;
    } else if (name == "rss") {
        stats.rss = value;
    } else if (name == "mapped_file") {
        stats.mapped_file = value;
    } else if (name == "pgpgin") {
        stats.pgpgin = value;
    } else if (name == "pgpgout") {
        stats.pgpgout = value;
    } else if (name == "swap") {
        stats.swap = value;
    } else if (name == "active_anon") {
        stats.active_anon = value;
    } else if (name == "inactive_anon") {
        stats.inactive_anon = value;
    } else if (name == "active_file") {
        stats.active_file = value;
    } else if (name == "inactive_file") {
        stats.inactive_file = value;
    } else if (name == "unevictable") {
        stats.unevictable= value;
    } else if (name == "hierarchical_memory_limit") {
        stats.hierarchical_memory_limit = value;
    } else if (name == "hierarchical_memsw_limit") {
        stats.hierarchical_memsw_limit = value;
    }

}

} // end namespace anonymous

//
// Get the more detailed memory use stats
// If this function throws an exception the outputs aren't written
//
void
ControlGroup::getMemoryStats(const std::string& groupPath,
                             ControlGroup::MemoryStats& memoryStats,
                             ControlGroup::MemoryStats& totalStats)
{
    void* handle;
    struct cgroup_stat statEntry;
    if (cgroup_read_stats_begin("memory", groupPath.c_str(), &handle, &statEntry) != 0) {
        throw runtime_error("Couldn't read the memory stats");
    }

    parseMemStatEntry(statEntry, memoryStats, false);
    parseMemStatEntry(statEntry, totalStats, true);
    while (cgroup_read_stats_next(&handle, &statEntry) == 0) {
        parseMemStatEntry(statEntry, memoryStats, false);
        parseMemStatEntry(statEntry, totalStats, true);
    }
    cgroup_read_stats_end(&handle);
}

//
// Update the cpu limit for the cgroup
// 
void
ControlGroup::changeCpuQuota(
    const std::string& groupPath,
    float cpuAmount)
{
    // create an empty cgroup structure since we only want to update one controller
    cgroup* arrasGroup = cgroup_new_cgroup(groupPath.c_str());
    if (arrasGroup == nullptr) {
        throw runtime_error("Couldn't create the group structure");
    }

    // add the cpu controller
    cgroup_controller* cpuController = cgroup_add_controller(arrasGroup, "cpu");
    if (cpuController== nullptr) {
        throw runtime_error("Couldn't create the cpu controller");
    }

    long cpuQuota;
    if (cpuAmount < 0.0) {
        cpuQuota = -1;
    } else {
        cpuQuota = static_cast<long>(CPU_PERIOD * cpuAmount);
    }
    cgroup_add_value_int64(cpuController, "cpu.cfs_quota_us", cpuQuota);

    // write out the changes
    int status = cgroup_modify_cgroup(arrasGroup);
    if (status != 0) {
        std::string description = std::string("Couldn't modify cgroup :") + cgroup_strerror(status);
        freeGroup(arrasGroup);
        throw runtime_error(description);
    }

    freeGroup(arrasGroup);
}

//
// Get the cpu quota taking the inheritence model into account
//
// If this function throws an exception the outputs aren't written
//
void
ControlGroup::getCpuQuota(
    const std::string& groupPath,
    float& cores,
    long& period, long& quota)
{
    std::string localPath = groupPath;

    // strip off a leading '/'
    if ((localPath.length() > 0) && (localPath[0]=='/')) {
        localPath = localPath.substr(1);
    }

    float localCores = 0.0;
    long localPeriod = 0;
    long localQuota = 0;
    do {
        getCpuQuotaRaw(localPath, localCores, localPeriod, localQuota);

        // keeo looking until there is an actual quota
        // (there will be an actual quota at the base if nothing else)
        if (localQuota > -1) return;
        size_t index = localPath.rfind('/');
        if (index != localPath.npos) {
            localPath = localPath.substr(0,index);
        } else if (localPath.length() == 0) {
            break;
        } else {
            localPath = "";
        }
    } while (1);

    // return the results
    cores = localCores;
    period = localPeriod;
    quota = localQuota;
}

//
// Get the cpu quota for the one cgroup without taking inheritence into account
// Mostly just used by getCpuQuota()
//
// If this function throws an exception then the outputs aren't written
//
void
ControlGroup::getCpuQuotaRaw(const std::string& groupPath, float& cores, long& period, long& quota)
{
    cgroup* group= getGroup(groupPath);
    cgroup_controller* cpuController = cgroup_get_controller(group, "cpu");
    if (cpuController == nullptr) {
        freeGroup(group);
        throw runtime_error("Error gettig cpu controller");
    }

    long cpuPeriod;
    if (cgroup_get_value_int64(cpuController, "cpu.cfs_period_us", &cpuPeriod) != 0) {
        throw runtime_error("Couldn't get cfs_period_us");
    }

    long cpuQuota;
    if (cgroup_get_value_int64(cpuController, "cpu.cfs_quota_us", &cpuQuota) != 0) {
        throw runtime_error("Couldn't get cfs_quota_us");
    }

    freeGroup(group);

    if ((cpuQuota >= 0) && (cpuPeriod > 0)) {
        cores = static_cast<float>(cpuQuota)/static_cast<float>(cpuPeriod);
    } else {
        cores = -1.0;
    }

    // return the results
    period= cpuPeriod;
    quota= cpuQuota;
}


//
// Only one thread may be in this function at a time for  given ControlGroup
// structure since it is doing a poll()
//
int
ControlGroup::waitOomStatus(std::vector<std::string>& groups, int milliSeconds)
{
    int count = 0;

    // if we think there are already groups out of memory
    // then just do the slow scan rather than waiting for
    // an event
    if (!mFastOomCheck) {
        count = scanOomStatus(groups);
        if (count > 0) return count;

        // since a brute force scan didn't show any out of memory groups
        // we can switch to waiting for events for this and future tries
        mFastOomCheck = true;
    }

    struct pollfd pollFd;
    pollFd.fd = mEventFd;
    pollFd.events = POLLIN;
    pollFd.revents = 0;

    // wait for out of memory events or until the requested timeout
    int status = poll(&pollFd, 1, milliSeconds);
    if (status <= 0) {
        return 0;
    } else {
        long eventData = -1;
        status = static_cast<int>(read(mEventFd, &eventData, sizeof(eventData)));
        if (status == 0) {
            // this would be a timeout or interruption with a signal
            return 0;
        } else if (status != 8) {
            throw runtime_error("There was an error polling the event file descriptor");
        } else if (eventData == 0) {
            // it doesn't seem like this is supposed to happen
            return 0;
        }

        count = scanOomStatus(groups);

        // generally count should be greater than 0 but if the process ran out of memory
        // but a still running thread released some then it could get back out of the
        // out of memory state
        if (count > 0) {
            mFastOomCheck = false;
        }
        return count;
    }
}

int
ControlGroup::scanOomStatus(std::vector<std::string>& groups)
const
{
    groups.clear();

    //
    // walk through the list and see who is out of memory
    //
    std::unique_lock<std::mutex> lock(mOomFdsMutex);

    std::map<std::string, int>::const_iterator iter = mOomFds.begin();
    while (iter != mOomFds.end()) {
        char buffer[50];
        lseek(iter->second, 0, SEEK_SET);
        int size = static_cast<int>(read(iter->second, buffer, 50));
        if (size > 0) {
            buffer[size] = 0;
            int status;
            if (sscanf(buffer,"%*s%*d%*s%d", &status) == 1) {
                if (status == 1) {
                    groups.push_back(iter->first);
                }
            }
        }
        ++iter;
    }

    int count = static_cast<int>(groups.size());
    return count;
}

//
// Add a group to the list of groups being monitored for out of memory events
//
void
ControlGroup::monitorOom(
    const std::string& groupName, bool enable)
{
    if (enable) {
        // make sure it doesn't already exist
        int oomFd = getOomFd(groupName);
        if (oomFd != -1) {
            return;
        }

        //
        // construct the paths to the oom_control file and the event_control file
        //
        char* path;
        int status = cgroup_get_subsys_mount_point("memory", &path);
        if (status != 0) {
            throw runtime_error(std::string("There is a problem with the cgroup memory mount point") + cgroup_strerror(status));
        }

        std::string oom_controlPath = std::string(path) + "/" +
                                      groupName + "/" +
                                      "memory.oom_control";
        std::string event_controlPath = std::string(path) + "/" +
                                       groupName + "/" +
                                       "cgroup.event_control";
        free(path);

        //
        // Open the oom_control file. This needs to stay opened for events to be processed
        // but won't actually bused by anything other than the c group memory controller for
        // noticing the oom events
        //
        oomFd = open(oom_controlPath.c_str(), O_RDONLY);
        if (oomFd < 0) {
            throw runtime_error(std::string("Couldn't open ") + oom_controlPath);
        }

        //
        // Write the mEventFd and the oom_control file descriptor the the event_control file
        // The event_control file doesn't need to stay opened
        //
        {
            int eventControlFd = open(event_controlPath.c_str(), O_WRONLY);
            if (eventControlFd< 0) {
                throw runtime_error(std::string("Couldn't open ") + event_controlPath);
            }
            std::string descriptors = std::to_string((long long)mEventFd) + " " + std::to_string((long long)oomFd);
            write(eventControlFd, descriptors.c_str(), descriptors.size());
            close(eventControlFd);
        }
    
        // remember the oom file descriptor so it can be closed when done monitoring
        if (!addOomFd(groupName, oomFd)) {
            // another thread beat us to it
            close(oomFd);
        }

        return;

    } else {
        int oomFd = getOomFd(groupName);
        if (oomFd == -1) {
            // it isn't being monitored
            return;
        }

        close(oomFd);

        removeOomFd(groupName);

        // There is no way to discontinue notification of oom events for the cgroup but
        // it will just appear as a spurious event since the cgroup has been removed from
        // the list. (i.e. there will be an oom event but scanning to find out which one
        // will turn up nothing so it will be ignored. The notification registration
        // will only be cancelled when the eventfd is closed (which would cancel all of
        // the events) or when the cgroup is removed (which is probably going to happen
        // next anyway so this probably isn't a problem.
        //
        // in /kernel/cgroup.c  cgroup_event_remove is called when the eventfd is closed.
        //                      cgroup_rmdir is called when the cgroup dir is removed
        // These will both call cgroup_event_remove() 

        return;
    }
}

//
// The subgroup function just wrap the function to prepend the base group name.
// except for waitOomStatusSubgroup which strips the base group name on the
// returned list
//
void
ControlGroup::createSubgroup(
    const std::string& computationName,
    unsigned long memLimitReal,
    unsigned long memLimitRealAndSwap,
    float cpuAmount)
const
{
    create(getFullGroupName(computationName), memLimitReal, memLimitRealAndSwap, cpuAmount, !mUseHierarchy);
}

void
ControlGroup::destroySubgroup(const std::string& computationName)
{
    destroy(getFullGroupName(computationName));
}

void
ControlGroup::addTaskSubgroup(const std::string& computationName, pid_t pid)
const
{
    addTask(getFullGroupName(computationName), pid);
}

void
ControlGroup::addSelfSubgroup(const std::string& computationName)
const
{
    if (!valid()) throw runtime_error("Invalid base group");

    addTaskSubgroup(computationName, getpid());
}

void
ControlGroup::changeMemoryLimitSubgroup(
    const std::string& computationName,
    unsigned long memLimitReal,
    unsigned long memLimitRealAndSwap)
const
{
    changeMemoryLimit(getFullGroupName(computationName), memLimitReal, memLimitRealAndSwap);
}

void
ControlGroup::getMemoryUsageSubgroup(const std::string& computationName,
                                             unsigned long& memoryReal,
                                             unsigned long& memoryRealAndSwap)
const
{
    getMemoryUsage(getFullGroupName(computationName), memoryReal, memoryRealAndSwap);
}

void
ControlGroup::getMemoryStatsSubgroup(const std::string& computationName,
                                     ControlGroup::MemoryStats& memoryStats,
                                     ControlGroup::MemoryStats& totalStats)
const
{
    getMemoryStats(getFullGroupName(computationName), memoryStats, totalStats);
}

void
ControlGroup::changeCpuQuotaSubgroup(
    const std::string& computationName,
    float cpuAmount)
{
    changeCpuQuota(getFullGroupName(computationName), cpuAmount);
}

void
ControlGroup::getCpuQuotaSubgroup(
    const std::string& computationName,
    float& cores,
    long& period, long& quota)
const
{
    getCpuQuota(getFullGroupName(computationName), cores, period, quota);
}

void
ControlGroup::getCpuQuotaRawSubgroup(
    const std::string& computationName,
    float& cores,
    long& period, long& quota)
const
{
    getCpuQuotaRaw(getFullGroupName(computationName), cores, period, quota);
}

namespace { // start anonymous namespace

void
stripPrefix(std::vector<std::string>& groups, const std::string& subString)
{
    size_t length = subString.length();
    auto iter = groups.begin();
    while (iter != groups.end()) {
        if (iter->substr(0,length) == subString) {
            *iter = iter->substr(length);
            iter++;
        } else {
            iter = groups.erase(iter);
        }
    }
}

} // end anonymous namespace

int
ControlGroup::waitOomStatusSubgroup(std::vector<std::string>& groups, int milliSeconds)
{
    if (!valid()) return 0;

    waitOomStatus(groups, milliSeconds);
    stripPrefix(groups, mBaseGroup + '/');
    return static_cast<int>(groups.size());
}

int
ControlGroup::scanOomStatusSubgroup(std::vector<std::string>& groups)
const
{
    if (!valid()) return 1;

    scanOomStatus(groups);
    stripPrefix(groups, mBaseGroup + '/');
    return static_cast<int>(groups.size());
}

void
ControlGroup::monitorOomSubgroup(const std::string& computationName, bool enable)
{
    monitorOom(getFullGroupName(computationName), enable);
}

//
// This is independent of other functions in this library
// Limits are deduced starting with just the process id.
//
// This will throw a runtime_error if there is a problem in which
// case the outputs won't be written.
//
void
ControlGroup::getTaskLimits(
    pid_t pid,
    unsigned long& memLimitReal,
    unsigned long& memLimitRealAndSwap,
    float& cpuLimit,
    long& period,
    long& quota)
{
    // open the processes cgroup file
    std::string cgroupFilePath = std::string("/proc/") + std::to_string((unsigned long long)pid) + "/cgroup";
    FILE* infile = fopen(cgroupFilePath.c_str(), "r");
    if (infile == nullptr) {
        throw runtime_error("Can't open the processes cgroup file");
    }
    std::string memoryGroupPath;
    std::string cpuGroupPath;
    while (!feof(infile)) { 
        char* line = nullptr;
        size_t count = 0;
        ssize_t size = getline(&line, &count, infile);
        if (size < 1) break;
        line[size-1] = 0;
        std::string cgroupLine(line);
        free(line);

        size_t first = cgroupLine.find(':');
        if (first == cgroupLine.npos) {
            fclose(infile);
            throw runtime_error(std::string("Error parsing ") + cgroupFilePath);
        }
        size_t second = cgroupLine.find(':', first+1);
        if (second == cgroupLine.npos) {
            fclose(infile);
            throw runtime_error(std::string("Error parsing ") + cgroupFilePath);
        }
        std::string controller(cgroupLine.substr(first+1, second-first-1));
        std::string path(cgroupLine.substr(second+1));

        if (controller == "memory") {
             memoryGroupPath = path;
        } else if (controller == "cpu") {
             cpuGroupPath = path;
        }
    }
    fclose(infile);

    // make sure the needed cgroups were provided
    if (memoryGroupPath.empty() || cpuGroupPath.empty()) {
        throw runtime_error("Couldn't get the cgroups paths");
    }

    MemoryStats stats;
    MemoryStats totalStats;
    getMemoryStats(memoryGroupPath, stats, totalStats);

    float localCpuLimit= -1.0;
    long localPeriod = -1;
    long localQuota = -1;
    getCpuQuota(cpuGroupPath, localCpuLimit, localPeriod, localQuota);

    // now that we've called everything that can through, fill in the outputs
    cpuLimit = localCpuLimit;
    period = localPeriod;
    quota = localQuota;
    memLimitReal = stats.hierarchical_memory_limit;
    memLimitRealAndSwap = stats.hierarchical_memsw_limit;
}

//
// private utility functions beyond this point
//
cgroup*
ControlGroup::getGroup(const std::string& groupPath)
{
    cgroup* arrasGroup = cgroup_new_cgroup(groupPath.c_str());
    if (arrasGroup == nullptr) {
        throw runtime_error("Couldn't create the group structure");
    }

    int status =  cgroup_get_cgroup(arrasGroup);
    if (status != 0) {
        freeGroup(arrasGroup);
        throw runtime_error("There was a problem getting " + groupPath + " cgroup (" + cgroup_strerror(status) + ")");
    }

    return arrasGroup;
}

//
// This is a trivial wrapper provided only for symmetry with getGroup
//
void
ControlGroup::freeGroup(cgroup* group)
{
    if (group != nullptr) {
        cgroup_free(&group);
    }
}

cgroup*
ControlGroup::getSubgroup(const std::string& computationName)
{
    return getGroup(getFullGroupName(computationName));
}

std::string
ControlGroup::getFullGroupName(const std::string& computationName) const
{
    if (!valid()) throw runtime_error("Invalid base group");

    return mBaseGroup + "/" + computationName;
}

//
// Maintaining the list of file descriptors for oom events. These are
// read when there is an oom event to figure out which group is out
// of memory
// 
bool
ControlGroup::addOomFd(const std::string& groupName, int oomFd)
{
    std::unique_lock<std::mutex> lock(mOomFdsMutex);

    std::map<std::string, int>::iterator iter = mOomFds.find(groupName);
    if (iter == mOomFds.end()) {
        // the entry doesn't exist add it
        mOomFds[groupName] = oomFd;
        return true;
    } else {
        // it already existed. nothing to do
        return false;
    }
}

int
ControlGroup::getOomFd(const std::string& groupName)
{
    std::unique_lock<std::mutex> lock(mOomFdsMutex);

    std::map<std::string, int>::iterator iter = mOomFds.find(groupName);
    if (iter == mOomFds.end()) {
        // the entry doesn't exist
        return -1;
    } else {
        return iter->second;
    }
}

void
ControlGroup::removeOomFd(const std::string& groupName)
{
    std::unique_lock<std::mutex> lock(mOomFdsMutex);

    std::map<std::string, int>::iterator iter = mOomFds.find(groupName);
    if (iter == mOomFds.end()) {
        // the entry doesn't exist. nothing to do
    } else {
        mOomFds.erase(iter);
    }
}

} // end namespace node
} // end namespace arras4

