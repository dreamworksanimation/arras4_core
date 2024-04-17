// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_EXECUTOR_HEARTBEATH__
#define __ARRAS4_EXECUTOR_HEARTBEATH__

#include <message_api/ContentMacros.h>
#include <string>

namespace arras4 {
    namespace impl {

class ExecutorHeartbeat : public arras4::api::ObjectContent
{
public:

    ARRAS_CONTENT_CLASS(ExecutorHeartbeat,"92c7ab1d-21a4-4cfe-a9fd-bd541436c15d",0);
    
    ExecutorHeartbeat() :
        mTransmitSecs(0),
        mTransmitMicroSecs(0),
        mThreads(0),
        mCpuUsage5SecsCurrent(0.0),
        mCpuUsage60SecsCurrent(0.0),
        mCpuUsageTotalSecs(0.0),
        mHyperthreaded(false),
        mMemoryUsageBytesCurrent(0),
        mSentMessages5Sec(0),
        mSentMessages60Sec(0),
        mSentMessagesTotal(0),
        mReceivedMessages5Sec(0),
        mReceivedMessages60Sec(0),
        mReceivedMessagesTotal(0)
       {}
  
    ~ExecutorHeartbeat() {}

    void serialize(arras4::api::DataOutStream& to) const override;
    void deserialize(arras4::api::DataInStream& from, unsigned version) override;

    // when was the heartbeat message sent
    uint64_t mTransmitSecs;
    uint64_t mTransmitMicroSecs;

    // cpu usage
    unsigned short mThreads;
    float mCpuUsage5SecsCurrent;
    float mCpuUsage60SecsCurrent;
    float mCpuUsageTotalSecs;
    bool mHyperthreaded;

    // memory usage
    size_t mMemoryUsageBytesCurrent;

    // messages sent
    uint64_t mSentMessages5Sec;
    uint64_t mSentMessages60Sec;
    uint64_t mSentMessagesTotal;
    uint64_t mReceivedMessages5Sec;
    uint64_t mReceivedMessages60Sec;
    uint64_t mReceivedMessagesTotal;

    // optional computation status
    std::string mStatus;
};

}
}

#endif
