// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PROCESS_EXIT_CODESH__
#define __ARRAS4_PROCESS_EXIT_CODESH__

#include <string>

namespace arras4 {
    namespace impl {

// these are the exit codes that execComp can return. They are defined here
// because they are shared between execComp, node and the client library
// Most of the numeric values are inherited from Arras3

struct ProcessExitCodes
{
    static const int NORMAL;                 //  0  Normal termination without error
    static const int INVALID_CMDLINE;        //  1  Invalid args to execComp
    static const int CONFIG_FILE_LOAD_ERROR; //  2  Couldn't read supplied config file
    static const int INVALID_CONFIG_DATA;    //  9  Something invalid in contents of config file
    static const int INITIALIZATION_FAILED;  //  8  Computation failed to initialize
    static const int EXEC_ERROR;             //  5  execv failed
    static const int COMPUTATION_LOAD_ERROR; //  6  Failed to load computation DSO
    static const int COMPUTATION_GO_TIMEOUT; //  7  Computation timed out waiting for "go"
    static const int EXCEPTION_CAUGHT;       // 13  Exception was thrown by computation or message code
    static const int UNSPECIFIED_ERROR;      // 14  Error not fitting any other category
    static const int DISCONNECTED;           // 20  IPC disconnected from computation
    static const int INTERNAL_ERROR;         // 21  Error in message format or computation state
};

std::string exitCodeString(int exitCode, bool expected);

}
}
#endif
