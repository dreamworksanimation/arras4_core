// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGE_QUEUEH__
#define __ARRAS4_MESSAGE_QUEUEH__

#include "ThreadsafeQueue.h"
namespace arras4 {
    namespace impl {

    class Envelope;
        
    using MessageQueue = ThreadsafeQueue<Envelope>;

}
}

#endif
