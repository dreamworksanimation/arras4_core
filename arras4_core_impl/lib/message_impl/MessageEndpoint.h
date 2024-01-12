// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGE_ENDPOINTH__
#define __ARRAS4_MESSAGE_ENDPOINTH__

#include "Envelope.h"
#include <message_api/messageapi_types.h>

namespace arras4 {
    namespace impl {

// MessageEndpoint combines a message reader and a message writer
class MessageEndpoint
{
public:
    virtual ~MessageEndpoint() {}
    virtual Envelope getEnvelope()=0;
    virtual void putEnvelope(const Envelope&)=0;
    virtual void shutdown()=0;
};

}
}
#endif
    
