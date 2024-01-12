// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PROCESS_CONTROLLER_H__
#define __ARRAS4_PROCESS_CONTROLLER_H__

#include <message_api/UUID.h>

namespace arras4 {
    namespace impl {

// a "controlled process" is a child process that exposes an additional interface via IPC and Arras messaging.
// The baseline is that 
//      - you can send a "stop" control message to the process asking it to shut down cleanup
//      - the process can send back "heartbeat" status messages with process stats
// Computations are a kind of controlled process that also support a "go" message containing outgoing message
// filters.
// There isn't any immediate plan to support controlled processes that aren't computations : the distinction is to
// do with which pieces of the service code require which features...
//
// ProcessController is implemented by ArrasController in arras4_node/lib/sessions
class ProcessController
{
public:
    virtual ~ProcessController() {}

    // send a stop control message to a computation.
    // return true if the message was sent,
    // false if the computation is not connected.
    virtual bool sendStop(const api::UUID& id,
                          const api::UUID& sessionId)=0;
};
    }
}
#endif
