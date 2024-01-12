// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ComputationHandle.h"
#include "ComputationLoadError.h"
#include <arras4_log/LogEventStream.h>
#include <computation_api/Computation.h>
#include <link.h>

#include <iostream>
#include <dlfcn.h>

namespace {
std::string safeDlerror() {
    char* dle = dlerror();
    if (dle)
        return dle;
    return "No dynamic loading error was reported";
}

struct LibData {
   std::string libraryPath;
   void* searchAddress;
};

int
dl_iterate_callback_find_address(struct dl_phdr_info *info, size_t, void *ptr)
{
    LibData* libData = reinterpret_cast<LibData*>(ptr);

    for (auto j = 0; j < info->dlpi_phnum; j++) {
        // store range in variabled for easier debug
        char* startAddr =  reinterpret_cast<char*>(info->dlpi_addr) + info->dlpi_phdr[j].p_vaddr;
        char* endAddr= startAddr + info->dlpi_phdr[j].p_memsz;

        // see if the search address is in the range
        if ((libData->searchAddress >= startAddr) && (libData->searchAddress < endAddr)) {
            libData->libraryPath = info->dlpi_name;

            // return a non-zero value to stop iterating over libraries
            return 1;
        }
    }
    return 0;
}

std::string
get_library_path(void* function_address = nullptr)
{
    LibData libData;

    // use the calling address if one isn't provided
    if (function_address == NULL) {
        libData.searchAddress = __builtin_return_address(0);
    } else {
        libData.searchAddress = function_address;
    }

    // traverse all of the libraries to look for the address
    dl_iterate_phdr(dl_iterate_callback_find_address, &libData);

    return libData.libraryPath;
}

}

namespace arras4 {
    namespace impl {


ComputationHandle::ComputationHandle(const std::string& dsoName,
                                     api::ComputationEnvironment* env) 
{
    typedef api::Computation* (*CREATE_FP)(api::ComputationEnvironment*);

    mDso = dlopen(dsoName.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!mDso) {
        std::string msg("Failed to load computation dso '" +
                        dsoName + "': " + safeDlerror());
        throw ComputationLoadError(msg);
    }

    CREATE_FP createFunc = (CREATE_FP)dlsym(mDso,COMPUTATION_CREATE_FUNC);
    if (!createFunc) {
        dlclose(mDso);
        std::string msg("Failed to load symbol '" + 
                        std::string(COMPUTATION_CREATE_FUNC) +
                        "' from computation dso '" +
                        dsoName + "': " + safeDlerror());
        throw ComputationLoadError(msg);
    }

    std::string fullPath = get_library_path(reinterpret_cast<void*>(createFunc));
    ARRAS_DEBUG(std::string("Computation dso path: ") << fullPath);

    mComputation = createFunc(env);
    if (!mComputation) {
        dlclose(mDso);
        std::string msg("Computation creation failed in computation dso '"+
                        dsoName + "': " + safeDlerror());
        throw ComputationLoadError(msg);
    }
}

ComputationHandle::~ComputationHandle()
{
    delete mComputation;
    dlclose(mDso);
}

}
}
