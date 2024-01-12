// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MessageQueue.h"
#include "ThreadsafeQueue_impl.h"
#include <message_impl/Envelope.h>

namespace arras4 {
    namespace impl {

template class ThreadsafeQueue<Envelope>;


}
}
