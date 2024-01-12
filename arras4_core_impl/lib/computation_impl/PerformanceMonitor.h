// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PERFORMANCE_MONITORH__
#define __ARRAS4_PERFORMANCE_MONITORH__

#include <condition_variable>
#include <mutex>
#include <message_api/Address.h>
#include <message_api/messageapi_types.h>

namespace {
    constexpr size_t ONE_MB = 1000000;
}

namespace arras4 {
    namespace impl {

        class MessageDispatcher;
        class ExecutionLimits;

class PerformanceMonitor 
{
public:
    // create a performance monitor, which can run in a thread
    // and send out 'ExecutorHeartbeat' messages.
    PerformanceMonitor(const ExecutionLimits& limits, 
                       MessageDispatcher& dispatcher,
                       const api::Address& from,
                       const api::AddressList& to)
        : mLimits(limits),
          mDispatcher(dispatcher), mRun(false),
          mFromAddress(from), mToList(to)
        {}

    void run();
    void stop();

private:
    const ExecutionLimits& mLimits;
    MessageDispatcher& mDispatcher;
    
    bool mRun;
    std::mutex mRunMutex;
    std::condition_variable mRunCondition;
    const api::Address mFromAddress;
    const api::AddressList mToList;
};
}
}
#endif

