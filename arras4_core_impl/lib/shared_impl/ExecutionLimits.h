// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_EXECUTION_LIMITSH__
#define __ARRAS4_EXECUTION_LIMITSH__

#include <message_api/Object.h>

#include <thread>

namespace {
    int constexpr DEFAULT_MEM_MB = 2048;
}

namespace arras4 {
    namespace impl {

// ExecutionLimits defines low-level system parameters
// that affect execution of a computation, like whether to
// use hyperthreading or maximum memory usage. 
class ExecutionLimits
{
public:
    // initialize with affinity disabled : call enableAffinity to
    // turn it on
    ExecutionLimits(unsigned aMaxMemoryMB,
                    unsigned aMaxCores,
                    unsigned aThreadsPerCore) :
        mUnlimited(false),
        mMaxMemoryMB(aMaxMemoryMB), 
        mMaxCores(aMaxCores),
        mThreadsPerCore(aThreadsPerCore), 
        mUseAffinity(false)
        {}

    // default is 'unlimited'
    ExecutionLimits() :
        mUnlimited(true),
        mMaxMemoryMB(DEFAULT_MEM_MB),
        mMaxCores(1),
        mThreadsPerCore(1),
        mUseAffinity(false)
        {}
    
    // set from a configuration object, optional fields are:
    // maxMemoryMB, maxCores, threadsPerCore,
    // useAffinity, cpuSet, hyperthreadCpuSet.
    // if useAffinity is true, cpuSet and hyperthreadCpuSet
    // are required. logs errors for any problems found in
    // the config.
    bool setFromObject(api::ObjectConstRef obj);

    // store configuration in an object, in a way that will be
    // correctly restored by setFromObject
    void toObject(api::ObjectRef obj);

    // apply these limits to the process and/or the computation 
    // message handler thread as appropriate.
    void apply(std::thread& handlerThread) const;

    // enable affinity and specify cpus as a list of comma-separated
    // integers with no spaces (e.g. "1,2,3,4,5,6").
    // If threadsPerCore = 1 then hyperthreadCpus will be ignored. 
    // If threadsPerCore > 1 then the two sets will be combined.
    // The length of cpus must be equal to 'maxCores'
    // The length of hyperthreadCpus must be maxCores*(threadsPerCore-1)
    // cpus and hyperthreadCpus must not overlap
    // returns true if conditions were satisfied and useAffinity/cpus was set
    // return false if there was a problem : then useAffinity will be unchanged 
    // and a error will have been logged.
    bool enableAffinity(const std::string& cpus,
                        const std::string& hyperthreadCpus);

    void disableAffinity() { mUseAffinity = false; }
    void disableHyperthreading() { mThreadsPerCore = 1; }

    bool unlimited() const { return mUnlimited; }
    void setUnlimited(bool flag) { mUnlimited = flag; }
    unsigned maxMemoryMB() const { return mMaxMemoryMB; }
    void setMaxMemoryMB(unsigned v) { mMaxMemoryMB = v; }
    unsigned maxCores() const { return mMaxCores; }
    void setMaxCores(unsigned v) { mMaxCores = v; }
    unsigned threadsPerCore() const { return mThreadsPerCore; }
    void setThreadsPerCore(unsigned v) { mThreadsPerCore = v; }
    unsigned maxThreads() const { return mMaxCores * mThreadsPerCore; }
    bool usesAffinity() const { return mUseAffinity; }
    bool usesHyperthreads() const { return mThreadsPerCore > 1; }

    //NOTE: the current node impl needs this : doesn't seem like it should be exposed..
    static bool setAffinityForProcess(const cpu_set_t& cpuSet, pid_t pid);

private:

    // if true, all limits are disabled and execution is unlimited
    bool mUnlimited;
    // the maximum amount of memory the computation is allowed to allocate, in MB
    unsigned mMaxMemoryMB;
    // the number of cores allocated to the computation
    unsigned mMaxCores;
    // the number of threads per core (> 1 => hyperthreading)
    unsigned mThreadsPerCore;
    // whether threads should be pinned to the processors 
    // specified in cpuSet using cpu affinity
    bool mUseAffinity;
    // the set of cpus to use, if mUseAffinity is true.
    cpu_set_t mCpuSet;
};

}
}
#endif
