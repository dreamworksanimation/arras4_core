// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ExecutionLimits.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <unistd.h>
#include <dirent.h>
#include <sched.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>

namespace {

// Utilities for cpu affinity
//
// Convert a comma separated list of processor indices into cpu_set_t
// List must be integers and commas, no white space (e.g. "1,2,3,4,5,6")
// Returns true on success and false on failure
//
bool procListToCpuSet(const std::string& procList, 
                      cpu_set_t& cpuSet,
                      unsigned requiredCount)
{
    CPU_ZERO(&cpuSet);
    size_t length = procList.length();
    if (length == 0) return false;
    size_t index = 0;
    unsigned count = 0;
    while (true) {
        // must start with a digit
        if (!isdigit(procList.at(index))) return false;
        int processor = 0;
        try {
            processor = std::stoi(procList.substr(index));
        } catch (...) { return false; }

        // must in the range of valid processor ids
        if ((processor < 0) || processor >= 1024) return false;
       
        CPU_SET(processor, &cpuSet);
        count++;

        index += std::to_string(processor).length();
        if (index == length) break;
        if (procList.at(index) != ',') return false;
        index++;
        if (index >= length) return false;
    }
    return (count == requiredCount);
}

std::string cpuSetToProcList(const cpu_set_t& cpuSet)
{
    std::string ret;
    for (unsigned i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i,&cpuSet)) {
            if (!ret.empty()) ret += ",";
            ret += std::to_string(i);
        }
    }
    return ret;
}

}

namespace arras4 {
    namespace impl {

/* static */ bool
ExecutionLimits::setAffinityForProcess(const cpu_set_t& cpuSet, pid_t pid)
{
    // Open the directory with the list of threads for this process
    // This should only fail if the process has gone away
    std::string dirname = "/proc/";
    dirname += std::to_string(pid);
    dirname += "/task";
    DIR* dir= opendir(dirname.c_str());
    if (dir == NULL) return false;

    struct dirent* entry;
    while ((entry= readdir(dir)) != NULL) {
        int threadNum = atoi(entry->d_name);
        // skip over the . and .. files
        if (entry->d_name[0] == '.') continue;

        int status = sched_setaffinity(threadNum, sizeof(cpuSet), &cpuSet);
        if (status < 0) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return true;
}

bool ExecutionLimits::setFromObject(api::ObjectConstRef obj)
{
    api::ObjectConstRef unlimited = obj["unlimited"];
    if (unlimited.isBool()) {
        mUnlimited =  unlimited .asBool();
    } else {
        mUnlimited = false;
    }

    api::ObjectConstRef maxMemMbVal = obj["maxMemoryMB"];
    if (maxMemMbVal.isInt() && (maxMemMbVal.asInt() > 0)) {
        mMaxMemoryMB = maxMemMbVal.asInt();
    } else if (!maxMemMbVal.isNull()) {
        ARRAS_ERROR(log::Id("invalidLimits") << 
                    "Invalid Config : Computation limit 'maxMemoryMB' must be a positive integer");
        return false;
    }
    api::ObjectConstRef maxCoresVal = obj["maxCores"];
    if (maxCoresVal.isInt() && (maxCoresVal.asInt() > 0)) {
        mMaxCores = maxCoresVal.asInt();
    } else if (!maxCoresVal.isNull()) {
        ARRAS_ERROR(log::Id("invalidLimits") << 
                    "Invalid Config : Computation limit 'maxCores' must be a positive integer");
        return false;
    }
    api::ObjectConstRef threadsPerCoreVal = obj["threadsPerCore"];
    if (threadsPerCoreVal.isInt()  && (threadsPerCoreVal.asInt() > 0)) {
        mThreadsPerCore = threadsPerCoreVal.asInt();
    } else if (!threadsPerCoreVal.isNull()) {
        ARRAS_ERROR(log::Id("invalidLimits") << 
                    "Invalid Config: Computation limit 'threadsPerCore' must be a positive integer");
        return false;
    }
    api::ObjectConstRef useAffinityVal = obj["useAffinity"];
    api::ObjectConstRef cpuSetVal = obj["cpuSet"];
    api::ObjectConstRef htCpuSetVal = obj["hyperthreadCpuSet"];
    if (useAffinityVal.isBool()) {
        if (useAffinityVal.asBool()) {
            std::string cpuSet;
            if (cpuSetVal.isString()) cpuSet = cpuSetVal.asString();
            std::string htCpuSet;
            if (htCpuSetVal.isString()) htCpuSet = htCpuSetVal.asString();
            enableAffinity(cpuSet,htCpuSet);
        } else {
            disableAffinity();
        }
    } else if (!useAffinityVal.isNull() || 
               !cpuSetVal.isNull() ||
               !htCpuSetVal.isNull()) {
        ARRAS_ERROR(log::Id("invalidLimits") << 
                    "Invalid Config : invalid cpu affinity settings for computation");
        return false;
    }
    return true;
}
  
void ExecutionLimits::toObject(api::ObjectRef obj)
{
    if (mUnlimited) {
        obj["unlimited"] = true;
        return;
    }
     
    obj["maxMemoryMB"] = mMaxMemoryMB;
    obj["maxCores"] = mMaxCores;
    obj["threadsPerCore"] = mThreadsPerCore;
    
    if (mUseAffinity) {
        obj["useAffinity"] = true;
        obj["cpuSet"] = cpuSetToProcList(mCpuSet);
    }
}   

bool ExecutionLimits::enableAffinity(const std::string& cpus,
                                     const std::string& hyperthreadCpus)
{
    cpu_set_t cpuSet;
    if (!procListToCpuSet(cpus,cpuSet,mMaxCores)) {
        ARRAS_ERROR(log::Id("invalidLimits") <<
                    "Invalid Config : invalid cpu affinity set for computation");
        return false;
    }
    if (mThreadsPerCore > 1) {
        // hyperthreading enabled
        cpu_set_t htCpuSet;
        if (!procListToCpuSet(hyperthreadCpus,htCpuSet,
                              mMaxCores*(mThreadsPerCore-1))) 
        {
            ARRAS_ERROR(log::Id("invalidLimits") <<
                        "Invalid Config : invalid hyperthread cpu affinity set for computation");
            return false;
        }
        cpu_set_t testSet;
        CPU_AND(&testSet,&cpuSet,&htCpuSet);
        if (CPU_COUNT(&testSet) != 0) {
            ARRAS_ERROR(log::Id("invalidLimits") <<
                        "Invalid Config : regular and hyperthread cpu affinity sets may not overlap");
            return false;
        }
        CPU_OR(&cpuSet,&cpuSet,&htCpuSet);
    }
    mUseAffinity = true;
    std::memcpy(reinterpret_cast<void*>(&mCpuSet),
                reinterpret_cast<void*>(&cpuSet),
                sizeof(cpu_set_t));
    return true;
}

void ExecutionLimits::apply(std::thread& /*handlerThread*/) const
{
    if (mUnlimited) return;
    // set affinity
    if (mUseAffinity) {
        // to maintain compatibility with Arras 3 behavior, we
        // apply affinity to the entire process at present
        setAffinityForProcess(mCpuSet, getpid());
    }
    // todo : apply memory limit
    // this limit has not been applied in previous Arras versions,
    // per comment: "Don't actually set the memory limit until we get some experience"
}
 
}
}
