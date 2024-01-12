// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __COMPUTATION_GROUP_H__
#define __COMPUTATION_GROUP_H__

#ifdef ARRAS_ENABLE_CGROUP
#include <libcgroup.h>
#else
#include "cgroup_stubs.h"
#endif
#include <map>
#include <mutex>
#include <string>   // for std::string
#include <vector>

//
// The philosophy of this library is that within a single application groups
// would tend to be peers and would all be within a single parent group which
// wouldn't likely be the root since creating groups there usually requires
// permission. To this end there is a Subgroup version of all the functions
// which use a base path so that the names can stay simple.
//
// There are more things that c groups can support than this library exposes.
// This library is sticking to the basics to keep it simple.

namespace arras4 {
namespace impl {

class ControlGroup {
  public:
    struct MemoryStats {
        MemoryStats() : cache(0), rss(0), mapped_file(0), pgpgin(0),
                        pgpgout(0), swap(0), active_anon(0), inactive_anon(0),
                        active_file(0), inactive_file(0), unevictable(0),
                        hierarchical_memory_limit(0), hierarchical_memsw_limit(0) {};
        unsigned long cache;
        unsigned long rss;
        unsigned long mapped_file;
        unsigned long pgpgin;
        unsigned long pgpgout;
        unsigned long swap;
        unsigned long active_anon;
        unsigned long inactive_anon;
        unsigned long active_file;
        unsigned long inactive_file;
        unsigned long unevictable;
        unsigned long hierarchical_memory_limit;
        unsigned long hierarchical_memsw_limit;
    };

    ControlGroup();

    // Set the base name prefixed on Subgroup versions of the functions
    // and confirm that cgroups can be created and destroyed there.
    void setBaseGroup(const std::string& baseGroup);

    // create a cgroup with the specified limits
    // cpuAmount is not the number of the threads to allow but the amount
    // of average cpu consumption to allow.
    // TODO make cpuAmount 0.0 or less means inherit limit from parent
    static void create(const std::string& groupName,
               unsigned long memLimitReal,        // memory limit in bytes
               unsigned long memLimitRealAndSwap, // memory limit including any swap
               float cpuAmount,                   // number of processors to use
               bool pauseOnOom=true);             // whether to pause or kill in out of memory case
    void createSubgroup(const std::string& computationName,
                       unsigned long memLimitReal,        // memory limit in bytes
                       unsigned long memLimitRealAndSwap, // memory limit including any swap
                       float cpuAmount) const;            // number of processors to use

    // destroy the c group. It must be empty.
    // Low level cgroup code allows a non-empty c group to be deleted but in
    // that case the tasks are moved to the parent c group which would mean
    // that any limits which were in place would be removed.
    void destroy(const std::string& computationName);
    void destroySubgroup(const std::string& computationName);

    // change the memory limit up or down to the new value
    static void changeMemoryLimit(
        const std::string& groupName,
        unsigned long memLimitReal,
        unsigned long memLimitRealAndSwap);
    void changeMemoryLimitSubgroup(
        const std::string& computationName,
        unsigned long memLimitReal,
        unsigned long memLimitRealAndSwap) const;


    // get the amount of memory and swap used by the c group 
    static void getMemoryUsage(const std::string& groupName,
                       unsigned long& memoryReal,
                       unsigned long& memoryRealAndSwap);
    void getMemoryUsageSubgroup(const std::string& computationName,
                       unsigned long& memoryReal,
                       unsigned long& memoryRealAndSwap) const;

    static void getMemoryStats(const std::string& groupPath,
                              ControlGroup::MemoryStats& memoryStats,
                              ControlGroup::MemoryStats& totalStats);
    void getMemoryStatsSubgroup(const std::string& computationName,
                              ControlGroup::MemoryStats& memoryStats,
                              ControlGroup::MemoryStats& totalStats) const;

    void changeCpuQuota(const std::string& groupPath,
                        float cpuAmount);
    void changeCpuQuotaSubgroup(const std::string& computationName,
                        float cpuAmount);

    // get the limits for the task
    // Raw=just check the group
    // non Raw= walk up hierarchy to find a quota if it is inherited from parent
    static void getCpuQuota(const std::string& groupPath, float& cores, long& period, long& quota);
    void getCpuQuotaSubgroup(
        const std::string& computationName,
        float& cores,
        long& period, long& quota) const;

    static void getCpuQuotaRaw(
        const std::string& groupPath,
        float& cores,
        long& period, long& quota);
    void getCpuQuotaRawSubgroup(
        const std::string& computationName,
        float& cores,
        long& period, long& quota) const;

    // add a task to the group or with the current thread/process.
    static void addTask(const std::string& groupName, pid_t pid);
    void addTaskSubgroup(const std::string& computationName, pid_t pid) const;
    static void addSelf(const std::string& groupName);
    void addSelfSubgroup(const std::string& computationName) const;

    // add or remove a cgroup to the list that us monitored for out memory status
    void monitorOom(const std::string& computationName, bool enable);
    void monitorOomSubgroup(const std::string& groupName, bool enable);

    // Brute force scan the monitored computations for out of memory computation
    // This is not as efficient as waitOomStatus and shouldn't generally be necessary.
    // computationNames is a vector of computation names
    // returns the number of groups with out of memory status
    int scanOomStatus(std::vector<std::string>& groupNames) const;
    int scanOomStatusSubgroup(std::vector<std::string>& computationNames) const;

    // groups is a vector of computation names or groups
    // milliseconds is how long to wait before returning
    //     -1 (the default) means wait forever until a group runs out of memory
    //     0 means return immediately
    //     other positive numbers are the number of milliseconds
    // returns the number of groups with out of memory status
    //
    // It is possible for it to return early with an empty list when there are
    // signals or certain race conditions but this shouldn't happen often.
    //
    // 1. a c group could go out of memory and then not be out of memory on its
    //    own if one of the other threads in the group frees memory
    // 2. If there is an out of memory event and another one happens while scanning
    //    for which group(s) are out of memory then the group could be found without
    //    processing the out of memory event. The out of memory event could then be
    //    resolved by the caller before calling to check again.
    // 3. The out of memory event may have come from a cgroup which has been removed
    //    from the monitoring list but there is no kernel API for stopping the events
    //    for that c group.
    //
    int waitOomStatus(std::vector<std::string>& groups, int milliSeconds=-1);

    // convenience function which waits for oom status and returns the subgroup name
    // groups not in the base group will not be returned
    int waitOomStatusSubgroup(std::vector<std::string>& groups, int milliSeconds=-1);


    const std::string& status() const { return mStatus; }
    bool valid() const {  return mValidBase; }

    static void getTaskLimits(
        pid_t pid,
        unsigned long& memoryLimitReal,
        unsigned long& memLimitRealAndSwap,
        float& cpuLimit,
        long& period,
        long& quota);

  private:
    static cgroup* getGroup(const std::string& groupPath);
    cgroup* getSubgroup(const std::string& computationName);
    std::string getFullGroupName(const std::string& computationName) const;
    static void freeGroup(cgroup* group);
    bool addOomFd(const std::string& groupName, int oomFd);
    int getOomFd(const std::string& groupName);
    void removeOomFd(const std::string& groupName);

    static std::mutex mInitializeMutex;
    static bool mInitialized;

    // a mapping between the cgroup name and the file descriptor for the
    // oom control file for manually checking the oom state
    std::map<std::string, int> mOomFds;
    mutable std::mutex mOomFdsMutex;
    int mEventFd;
    int mInitializeStatus;
    bool mFastOomCheck;
    bool mUseHierarchy;
    std::string mBaseGroup;
    std::string mStatus;
    bool mValid;
    bool mValidBase;
};

} // end namespace node
} // end namespace arras4

#endif // __COMPUTATION_GROUP_H

