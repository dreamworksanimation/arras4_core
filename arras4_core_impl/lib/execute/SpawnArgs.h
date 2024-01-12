// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_SPAWN_ARGS_H__
#define __ARRAS4_SPAWN_ARGS_H__

#include "Environment.h"

#include <vector>
#include <string>
#include <memory>

namespace arras4 {
    namespace impl {

class IoCapture;
class ProcessObserver;

// arguments passed to "spawn" when starting a process
class SpawnArgs
{
public:
    std::string program;                  // executable path
    std::vector<std::string> args;        // program args
    Environment environment; 
    std::string workingDirectory;
    bool cleanupProcessGroup = false;     
    // if set, all processes in the same group will be destroyed
    // when the process terminates
    std::shared_ptr<IoCapture> ioCapture;
    // set to non-null to capture stdout and stderr
    std::shared_ptr<ProcessObserver> observer;
    // sset to non-null to receive status updates
    bool enforceMemory = false;
    bool enforceCores = false;
    unsigned assignedMb = 0;
    unsigned assignedCores = 0; 

    // set our working directory to match the current process
    void setCurrentWorkingDirectory();

    std::string debugString(unsigned maxEnvirVars = 0,
                            bool options = false) const;

    // try to find the given executable name on the PATH
    // in our environment. If found, use it to set 'program' and
    // return true. OW leave 'program' unchanged and return false
    bool findProgramInPath(const std::string& name);

};

}
}
#endif
