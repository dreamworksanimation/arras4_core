// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGE_HANDLERH__
#define __ARRAS4_MESSAGE_HANDLERH__

#include <message_api/messageapi_types.h>

namespace arras4 {
    namespace impl {

class MessageHandler
{
public:
    virtual ~MessageHandler() {}
    virtual void handleMessage(const api::Message& message)=0;
    virtual void onIdle()=0;
};

}
}
#endif
