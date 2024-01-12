// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ControlMessageEndpoint.h"
#include <message_impl/Envelope.h>
#include <core_messages/ControlMessage.h>

namespace arras4 {
    namespace impl {

 
Envelope ControlMessageEndpoint::getEnvelope()
{
    // process messages until we get one that
    // _isn't_ a control message
    while (true) {
        Envelope env = mSource.getEnvelope();
        if (!processControlMessage(env)) {
            return env;
        }
    }
}

// if msg is a control message, forward command to mControlled 
// and return true. If it is a regular message, return false
bool ControlMessageEndpoint::processControlMessage(const Envelope& env)
{
    if (env.classId() == ControlMessage::ID) {
         ControlMessage::ConstPtr content = env.contentAs<ControlMessage>();
         if (content) {
             mControlled.controlMessage(content->command(),
		                        content->data());
         }
         return true;
    }
    return false;
}

}
}

  
