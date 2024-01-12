// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ExecutorHeartbeat.h"

namespace arras4 {
    namespace impl {

ARRAS_CONTENT_IMPL(ExecutorHeartbeat);

void ExecutorHeartbeat::serialize(api::DataOutStream& to) const
{
    to << mTransmitSecs << mTransmitMicroSecs;

    to << mThreads 
       << mCpuUsage5SecsCurrent << mCpuUsage60SecsCurrent << mCpuUsageTotalSecs
       << mHyperthreaded;

    to << mMemoryUsageBytesCurrent;

    to << mSentMessages5Sec << mSentMessages60Sec << mSentMessagesTotal;
    to << mReceivedMessages5Sec << mReceivedMessages60Sec << mReceivedMessagesTotal;

    to << mStatus;
}

void ExecutorHeartbeat::deserialize(api::DataInStream& from, 
                                 unsigned /*version*/)
{
    from >> mTransmitSecs >> mTransmitMicroSecs;

    from >> mThreads 
       >> mCpuUsage5SecsCurrent >> mCpuUsage60SecsCurrent >> mCpuUsageTotalSecs
       >> mHyperthreaded;

    from >> mMemoryUsageBytesCurrent;

    from >> mSentMessages5Sec >> mSentMessages60Sec >> mSentMessagesTotal;
    from >> mReceivedMessages5Sec >> mReceivedMessages60Sec >> mReceivedMessagesTotal;

    from >> mStatus;
}

}
}
 
