// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_DISPATCHER_EXIT_REASONH__
#define __ARRAS4_DISPATCHER_EXIT_REASONH__

#include <string>

namespace arras4 {
    namespace impl {

// The possible reasons a dispatcher might stop running.
enum class DispatcherExitReason {
    None,          // None .. it's still running
    Quit,          // Stop/quit request was invoked by another thread
    Disconnected,  // Socket disconnected
    MessageError,  // exception in message read/write impl
    HandlerError   // error return or exception in onMessage or onIdle        
};

const std::string& dispatcherExitReasonAsString(DispatcherExitReason der);

}
}
#endif
