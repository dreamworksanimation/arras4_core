// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ProcessExitCodes.h"

namespace arras4 {
    namespace impl {

        const int ProcessExitCodes::NORMAL                 =  0;
        const int ProcessExitCodes::INVALID_CMDLINE        =  1;
        const int ProcessExitCodes::CONFIG_FILE_LOAD_ERROR =  2;
        const int ProcessExitCodes::INVALID_CONFIG_DATA    =  9;
        const int ProcessExitCodes::INITIALIZATION_FAILED  =  8;
        const int ProcessExitCodes::COMPUTATION_LOAD_ERROR =  6;
        const int ProcessExitCodes::COMPUTATION_GO_TIMEOUT =  7;
        const int ProcessExitCodes::EXEC_ERROR             =  5;
        const int ProcessExitCodes::EXCEPTION_CAUGHT       = 13;
        const int ProcessExitCodes::UNSPECIFIED_ERROR      = 14;
        const int ProcessExitCodes::DISCONNECTED           = 20;
        const int ProcessExitCodes::INTERNAL_ERROR         = 21;

std::string exitCodeString(int ec, bool expected)
{
    switch (ec) {
    case ProcessExitCodes::NORMAL:
        if (expected) return "exited normally";
	else return "exited unexpectedly with code 0";
    case ProcessExitCodes::INVALID_CMDLINE:
        return "exited due to an invalid commandline";
    case ProcessExitCodes::CONFIG_FILE_LOAD_ERROR:
        return "failed to load the configuration file";
    case ProcessExitCodes::INVALID_CONFIG_DATA:
        return "exited due to an invalid configuration data";
    case ProcessExitCodes::INITIALIZATION_FAILED:
        return "failed to initialize properly";
    case ProcessExitCodes::COMPUTATION_LOAD_ERROR:
        return "failed to load the computation dso";
    case ProcessExitCodes::COMPUTATION_GO_TIMEOUT:
        return "timed out waiting for a 'go' signal";
    case ProcessExitCodes::EXCEPTION_CAUGHT:
        return "threw an exception (see log for details)";
    case ProcessExitCodes::EXEC_ERROR:
        return "failed to start the program (error in execv())";
    case ProcessExitCodes::DISCONNECTED:
        return "was disconnected";
    case ProcessExitCodes::INTERNAL_ERROR:
        return "exited due to a computation or message problem";
    case ProcessExitCodes::UNSPECIFIED_ERROR:
        return "exited due to an unspecified error";
    }
    return "exited with code "+std::to_string(ec);
}

}
}
