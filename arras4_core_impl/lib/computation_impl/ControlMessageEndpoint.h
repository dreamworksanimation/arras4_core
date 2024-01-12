// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CONTROL_MESSAGE_ENDPOINTH__
#define __ARRAS4_CONTROL_MESSAGE_ENDPOINTH__

#include <message_impl/MessageEndpoint.h>

// This is a message endpoint filter that directly handles control messages
// (e.g. "go", "stop") instead of passing them on to the message
// dispatcher. This means that they are handled immediately on 
// receipt, instead of being placed on the incoming message queue.
// This is particularly important for the "go" control message, since
// regular message dispatch doesn't even start running until 'go' is
// received.

namespace arras4 {
    namespace impl {

// interface for something controlled by a ControlMessageEndpoint.
// Generally CompEnvironmentImpl, and the control messages are
// starting and stopping the computation.
class Controlled {
public:
    virtual ~Controlled() {}
    virtual void controlMessage(const std::string& command,
	                        const std::string& data)=0;
};


// The endpoint filter itself.
// 'mSource' is generally a PeerMessageEndpoint reading directly from
// a socket. The caller of get/putMessage is generally MessageDispatcher. 
class ControlMessageEndpoint : public MessageEndpoint
{
public:
    ControlMessageEndpoint(MessageEndpoint& source,
                           Controlled& controlled) :
        mSource(source), mControlled(controlled)
        {}
    ~ControlMessageEndpoint() {}

    Envelope getEnvelope();
    void putEnvelope(const Envelope& env) { mSource.putEnvelope(env); }
    void shutdown() { mSource.shutdown(); }

private:
    bool processControlMessage(const Envelope& env);

    MessageEndpoint& mSource;
    Controlled& mControlled;
};

}
}
#endif
    
