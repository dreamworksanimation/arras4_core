// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "PerformanceMonitor.h"

#include <shared_impl/MessageDispatcher.h>
#include <shared_impl/ExecutionLimits.h>
#include <message_impl/Envelope.h>
#include <arras4_log/Logger.h>
#include <core_messages/ExecutorHeartbeat.h>

#include <unistd.h>
#include <sys/time.h>
#include <string>
#include <fstream>

namespace {
       
void getCpuUsage(unsigned long& ticks, long& num_threads)
{
   std::ifstream input("/proc/self/stat");

   // skip 13 fields
   for (int i=0; i < 13; i++) {
       std::string field;
       std::getline(input, field, ' ');
   }
   
   unsigned long user_ticks;
   input >> user_ticks;
   unsigned long system_ticks;
   input >> system_ticks;
   long children_user_ticks;
   input >> children_user_ticks;
   long children_system_ticks;
   input >> children_system_ticks;
   long priority;
   input >> priority;
   long nice;
   input >> nice;
   // num_threads declared as parameter
   input >> num_threads;
   // there is more in the file but we don't need that

   ticks = user_ticks + system_ticks;
}

size_t getMemUsage()
{
     // get the current memory usage
    size_t memoryUsagePages = 0;
    {
        //
        // Format for /proc/[pid]/statm
        //
        //    size       total program size
        //               (same as VmSize in /proc/[pid]/status)
        //    resident   resident set size
        //               (same as VmRSS in /proc/[pid]/status)
        //    share      shared pages (from shared mappings)
        //    text       text (code)
        //    lib        library (unused in Linux 2.6)
        //    data       data + stack
        //    dt         dirty pages (unused in Linux 2.6)

        std::ifstream input("/proc/self/statm");
        input >> memoryUsagePages;
    }
    return memoryUsagePages * 4096; 
}

} // namespace {

namespace arras4 {
    namespace impl {
 
void PerformanceMonitor::stop()
{
    std::unique_lock<std::mutex> lock(mRunMutex);
    mRun = false;
    mRunCondition.notify_all();
}

void PerformanceMonitor::run()
{
    {
        std::unique_lock<std::mutex> lock(mRunMutex);
        mRun = true;
    }

    double ticksPerSecond = (double)sysconf(_SC_CLK_TCK);
   
    unsigned long lastTicks;
    double times[12];
    int index=0;
    long threads=0;
    unsigned long lastSentMessages = 0;
    unsigned long lastReceivedMessages = 0;
    unsigned long sentMessages[12];
    unsigned long receivedMessages[12];

    // clear out the times
    for (int i=0; i< 12; i++) {
        sentMessages[i] = 0;
        receivedMessages[i] = 0;
        times[i] = 0.0;
    }

    // get a starting point for CPU usage
    getCpuUsage(lastTicks, threads);

    while (true) {
 
        auto heartbeat = std::make_shared<ExecutorHeartbeat>();

        heartbeat->mMemoryUsageBytesCurrent = getMemUsage();

        // get stats over the past 5 seconds and add it to a rolling buffer
        unsigned long totalTicks;
        getCpuUsage(totalTicks, threads);

        unsigned long totalSentMessages = mDispatcher.sentMessageCount();
        unsigned long totalReceivedMessages = mDispatcher.receivedMessageCount();

        double intervalCpuSeconds = ((double)(totalTicks - lastTicks)) / ticksPerSecond;
        unsigned long intervalSentMessages = totalSentMessages - lastSentMessages;
        unsigned long intervalReceivedMessages = totalReceivedMessages - lastReceivedMessages;
        lastTicks = totalTicks;
        lastSentMessages = totalSentMessages;
        lastReceivedMessages = totalReceivedMessages;
        times[index % 12] = intervalCpuSeconds;
        sentMessages[index % 12] = intervalSentMessages;
        receivedMessages[index % 12] = intervalReceivedMessages;
        index++;

        heartbeat->mHyperthreaded = mLimits.usesHyperthreads();

        // total rolling stats over the last 60 seconds
        double oneMinuteCpuSeconds = 0;
        unsigned long oneMinuteSentMessages = 0;
        unsigned long oneMinuteReceivedMessages = 0;
        for (int i=0; i<12; i++) {
            oneMinuteCpuSeconds += times[i];
            oneMinuteSentMessages += sentMessages[i];
            oneMinuteReceivedMessages += receivedMessages[i];
        }
     
        heartbeat->mCpuUsage5SecsCurrent = (float)(intervalCpuSeconds);
        heartbeat->mCpuUsage60SecsCurrent = (float)(oneMinuteCpuSeconds);
        heartbeat->mCpuUsageTotalSecs = (float)(((double)totalTicks) / ticksPerSecond);
        heartbeat->mThreads = (unsigned short)threads;

        struct timeval sendtime;
        sendtime.tv_sec = 0;
        sendtime.tv_usec = 0;

        // don't care if it fails. It's unlikely and there will be nothing we can do about it.
        // Just let the time stay at zero in that case.
        gettimeofday(&sendtime, nullptr);
        heartbeat->mTransmitSecs = sendtime.tv_sec;
        heartbeat->mTransmitMicroSecs = (unsigned int)(sendtime.tv_usec);

        // message statistics
        heartbeat->mSentMessages5Sec = intervalSentMessages;
        heartbeat->mSentMessages60Sec = oneMinuteSentMessages;
        heartbeat->mSentMessagesTotal = totalSentMessages;
        heartbeat->mReceivedMessages5Sec = intervalReceivedMessages;
        heartbeat->mReceivedMessages60Sec = oneMinuteReceivedMessages;
        heartbeat->mReceivedMessagesTotal = totalReceivedMessages;
        
        Envelope env(heartbeat);
        env.metadata()->from() = mFromAddress;
        env.to() = mToList;
        mDispatcher.send(env);

        // wait for 5 seconds before next iteration, unless
        // mRun becomes false
        {
            std::unique_lock<std::mutex> lock(mRunMutex);

            auto timeoutTime = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (mRun && (timeoutTime > std::chrono::steady_clock::now())) {
                mRunCondition.wait_until(lock, timeoutTime);
            }
            if (!mRun) break;
        }
    }
}

}
}
