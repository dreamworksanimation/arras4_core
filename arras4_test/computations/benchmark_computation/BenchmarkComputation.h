// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/*
    Messages recieved by BenchmarkCompution

    BenchmarkMessage
        mFrom
            the name of the computation (or client) which send the message for messages which want a
            reply (SEND_REPORT, SEND_ACK)
        mType
            NOOP - toss out the message
            ACK - This is an acknowledgement of reception of another message.
                  Used to manage credits for the communications.
            SEND_ACK - this is essentially a NOOP which should be acknowleged
                  The mValue will usually just be a large payload for bandwidth testing
            START_STREAM_OUT - launch a separate thread which will send data out
                mValue is a string with {credits} {datasize} {routingname}
                    {credits} is the number of unacknowledged messages which can be sent
                    {datasize} is the size of the mValue which should be set on the message
                    {routingname} is the routing name to use for the message
            LOGSPEED - launch threads which write a bunch of of messages to stderr
            PRINTENV - send all of the environment variables to the requester as SEND_REPORT messages

            SEND_REPORT
                Requests a bandwidth report when in StreamOut mode is enabled
        mValue
		The payload for SEND_ACK. It just bulks up the message for bandwidth testing.
                It is the parameters for START_STREAM_OUT (see above)

    
    Messages sent by BenchmarkComputation

    BenchmarkMessage

        mType
            SEND_ACK
                When StreamOut mode is enabled these are sent to create a stream of data

            ACK
                Acknowledge the recieve of a SEND_ACK from someone else

            REPORT
                A reponse to SEND_REPORT which has a report string in mValue
*/

#ifndef __ARRAS4_BENCHMARK_COMPUTATIONH__
#define __ARRAS4_BENCHMARK_COMPUTATIONH__

#include "Credits.h"

#include <atomic>
#include <computation_api/Computation.h>
#include <string>
#include <thread>

namespace arras4 {
    namespace benchmark {

class BenchmarkComputation : public arras4::api::Computation
{
public:
    BenchmarkComputation(arras4::api::ComputationEnvironment* env) :
        arras4::api::Computation(env) 
	  , mDataSize(0), mLogThreadsCount(0), mLogCount(0)
        {}

    arras4::api::Result onMessage(const arras4::api::Message& message);
    
    void onIdle();
    void onStart();
    void onStop();

    arras4::api::Result configure(const std::string& op,
                                  arras4::api::ObjectConstRef& config);

    void streamOut(int index);
    void startStreamOut(const std::string& params);
    void stopStreamOut();
    void reportStreamOut(const std::string& destination);

    void consumeCpu();
    void startConsumeCpu(unsigned int threads);
    void stopConsumeCpu();

    void consumeMemory();
    void startConsumeMemory();
    void stopConsumeMemory();

    void logSpeedThread(unsigned int threadIndex, unsigned int messageCount);
    void logSpeedTest(unsigned int threadCount, unsigned int messageCount);


private:
    Credits mCreditArray[16];
    std::string mName;
    std::string mNames[16];
    unsigned long mDataSize;

    std::atomic<bool> mStreamOutMode{false};
    std::thread mStreamOutThreads[16];
    std::string mStreamOutDestinations[16];
    unsigned long mStreamOutCounts[16] = { 0 };
    unsigned long mStreamOutLastCounts[16] = { 0 };

    unsigned int mLogThreadsCount;
    unsigned int mLogCount;

    std::atomic<bool> mConsumeCpuMode{false};
    unsigned int mConsumeCpuThreadCount = 0;
    std::vector<std::thread> mConsumeCpuThreads;

    std::atomic<bool> mConsumeMemoryMode{false};
    bool mConsumeMemoryTouchOnce = false;
    unsigned int mConsumeMemoryAllocateMb = 0;
    unsigned int mConsumeMemoryTouchMb = 0;
    std::thread mConsumeMemoryThread;
    std::vector<unsigned char*> mConsumeMemoryChunks;

    std::chrono::time_point<std::chrono::steady_clock> mStreamOutStartTimes[16];
    std::chrono::time_point<std::chrono::steady_clock> mStreamOutLastTimes[16];

    unsigned int mSleepInOnStop = 0;
};

} // end namespace benchmark
} // end namespace arras4
#endif
