// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "BenchmarkComputation.h"
#include "Credits.h"

#include <arras4_log/Logger.h>
#include <benchmark_message/BenchmarkMessage.h>
#include <computation_api/standard_names.h>
#include <cstring>
#include <iomanip>
#include <message_api/Object.h>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>   // for sleep(), environ

extern char** environ; // declare environ in global scope

using namespace arras4::api;

namespace arras4 {
    namespace benchmark {

COMPUTATION_CREATOR(BenchmarkComputation);

//
// Run a thread to consume and touch memory
// This has two uses:
//   1. Having caches dirtied by a thread may change the performance of some things
//   2. This can help test memory consumption monitoring and limiting
//

//
// Uselessly consume and touch memory
//
void
BenchmarkComputation::consumeMemory()
{
    int counter = 0;
    ARRAS_LOG_INFO("consumeMemory starting");

    // Allocate the requested memory. Often the system will allow
    // more memory to be allocate than is available as long as it
    // is never touched
    for (auto i = 0; i < mConsumeMemoryAllocateMb; i++) {
        unsigned char* ptr;
        try {
            ptr = new unsigned char[1048576];
        } catch (std::bad_alloc&) {
            ARRAS_LOG_ERROR("Failed to allocate memory after %du megabytes", i);
            mConsumeMemoryAllocateMb = i;
            break;
        }
        mConsumeMemoryChunks.push_back(ptr);

        // abort early if shutting down
        if (!mConsumeMemoryMode) break;
    }
    ARRAS_LOG_INFO("Done allocating %uMB of memory", mConsumeMemoryAllocateMb);

    // can't touch more than was allocated
    if (mConsumeMemoryTouchMb > mConsumeMemoryAllocateMb) {
        mConsumeMemoryTouchMb = mConsumeMemoryAllocateMb;
    }

    // Continuously touch the memory
    unsigned char value = 0;
    bool neverTouched= true;
    while (mConsumeMemoryMode) {
        for (auto i = 0; i < mConsumeMemoryTouchMb; i++) {
            memset(mConsumeMemoryChunks[i], value, 1048576);

            // abort early if shutting down
            if (!mConsumeMemoryTouchMb) break;
        }

        // go ahead an exit if it was TouchOnce mode
        if (mConsumeMemoryTouchOnce) return;

        // Value will wrap. We just need it to be changing.
        // It doesn't really matter what it is.
        value++;

        if (neverTouched) {
            ARRAS_LOG_INFO("Done touching %uMB of memory", mConsumeMemoryTouchMb);
            neverTouched = false;
        }
    }

    ARRAS_LOG_INFO("consumeMemory stopping");
}

//
// start memory consumption if it isn't already running
//
void
BenchmarkComputation::startConsumeMemory()
{
    if (!mConsumeMemoryMode) {
        mConsumeMemoryMode = true;
        mConsumeMemoryThread = std::thread(&BenchmarkComputation::consumeMemory, this);
    }
}

//
// stop memory consumption if it is running
//
void
BenchmarkComputation::stopConsumeMemory()
{
    if (mConsumeMemoryMode) {
        mConsumeMemoryMode = false;

        if (mConsumeMemoryThread.joinable()) mConsumeMemoryThread.join();

        // clean up the allocated memory
        for (auto iter = mConsumeMemoryChunks.begin(); iter != mConsumeMemoryChunks.end(); iter++) {
            delete[] *iter;
        }
        mConsumeMemoryChunks.clear();
    }

}

//
// Run some computation threads to use up CPU.
// This has two uses:
//   1. Having a busy system can change how fast messaging threads wake up
//      which will change latency and bandwidth of messaging
//   2. This can help in testing the CPU consumption monitoring and
//      limiting
//

//
// Uselessly consume CPU
//
void
BenchmarkComputation::consumeCpu()
{
    int counter = 0;
    ARRAS_LOG_INFO("consumeCpu starting");

    while (mConsumeCpuMode) {
        counter++;
    }

    ARRAS_LOG_INFO("consumeCpu stopping");
}

//
// start cpu consumption if it isn't already running
//
void
BenchmarkComputation::startConsumeCpu(unsigned int threads)
{
    if (!mConsumeCpuMode) {
        mConsumeCpuMode = true;
        for (int i=0; i < threads; i++) {
            mConsumeCpuThreads.push_back(std::thread(&BenchmarkComputation::consumeCpu, this));
        }
    }
}

//
// stop cpu consumption if it is running
//
void
BenchmarkComputation::stopConsumeCpu()
{
    if (mConsumeCpuMode) {
        mConsumeCpuMode = false;

        for (size_t i=0; i<mConsumeCpuThreads.size(); i++) {
            if (mConsumeCpuThreads[i].joinable()) mConsumeCpuThreads[i].join();
        }
        mConsumeCpuThreads.clear();
    }
}

//
// Use a separate thread to stream data to the client or another computation as
// fast as it can using the specified data size and message credit limit
// The received ACK messages will be handled by onMessage incrementing
// mCredit.
//

void
BenchmarkComputation::streamOut(int index)
{
    fprintf(stderr,"streamOut thread started %d\n",index);
    api::Object destinationOptions;

    destinationOptions[api::MessageOptions::routingName] = mStreamOutDestinations[index];

    while (mStreamOutMode) {
        BenchmarkMessage::Ptr message= std::make_shared<BenchmarkMessage>(BenchmarkMessage::SEND_ACK, "", mName);
        message->mValue.resize(mDataSize,'*');

        // block until there is a send credit available
        mCreditArray[index].waitAndDecrement();
        send(BenchmarkMessage::Ptr(message),destinationOptions);

        // keep track of the number of messages send for statistics
        mStreamOutCounts[index]++;
    }
}

//
// If it's not streaming out then start it
//
void
BenchmarkComputation::startStreamOut(const std::string& params)
{
    unsigned long credits;

    if (!mStreamOutMode) {
        mStreamOutMode = true;

        //
        // parse the params {credits} {datasize} {destination}
        //
        size_t firstSpace = params.find(' ');
        size_t secondSpace = params.find(' ', firstSpace+1);
        credits = std::stol(params.substr(0,firstSpace));
        mDataSize = std::stol(params.substr(firstSpace + 1, secondSpace - firstSpace - 1));

        for (auto i = 0; i < 16; i++) {
            mStreamOutCounts[i] = 0;
        }

        int index;
        size_t nextSpace;
        do {
            std::string destination;
            nextSpace = params.find(' ', secondSpace+1);
            if (nextSpace == std::string::npos) {
                destination = params.substr(secondSpace + 1);
            } else {
                destination = params.substr(secondSpace + 1, nextSpace - secondSpace - 1);
            }
            if (destination == "client") {
                index = 0;
            } else {
                index = std::stol(destination.substr(13));
                if (index > 15) continue;
            }
            mStreamOutDestinations[index] = destination;
            secondSpace = nextSpace;

            //
            // reset the number of send messages, initialize the credit system and get the start time
            //
            mStreamOutCounts[index] = 0;
            mCreditArray[index].set(credits);
            mStreamOutStartTimes[index] = std::chrono::steady_clock::now();
            mStreamOutLastTimes[index] = mStreamOutStartTimes[index];

            // start the thread
            mStreamOutThreads[index] = std::thread(&BenchmarkComputation::streamOut, this, index);

        } while (nextSpace != std::string::npos);
    }
}

//
// If it's streaming out then stop it
//
void
BenchmarkComputation::stopStreamOut()
{
    if (mStreamOutMode) {
        // setting this to false will allow the thread's loop to exit
        mStreamOutMode = false;
    
        for (auto i=0; i < 16; i++) {
            // make sure it does't get stuck waiting for a credit
            // since this is getting called from onMessage which
            // won't process more messages until this returns.
            mCreditArray[i].increment();

            // reap the thread
            if (mStreamOutThreads[i].joinable()) mStreamOutThreads[i].join();
        }
    }
}

namespace {

std::string bandwidth_report(
    float seconds,
    unsigned long messages,
    unsigned long data)
{
    std::ostringstream reportOut;

    reportOut << "Time: ";
    reportOut << std::fixed << std::setprecision(2) << seconds;
    reportOut << "s Msgs: ";
    reportOut << messages;
    reportOut << " Rate: ";
    reportOut << std::fixed << std::setprecision(2) << (messages / seconds);
    reportOut << "msg/s (";
    reportOut << std::fixed << std::setprecision(2) << 1.0/(messages / seconds)*1000000.0;
    reportOut << "µs) ";
    reportOut << std::fixed << std::setprecision(2) << (data / 1048576.0 / seconds);
    reportOut << "MB/s";

    return reportOut.str();
}

}

//
// Send a report if stream out is running
//
void
BenchmarkComputation::reportStreamOut(const std::string& destination)
{
    if (mStreamOutMode) {
        
        api:Object options;
        std::string routingName = std::string("For_") + destination;
        options[api::MessageOptions::routingName] = routingName;


        // snap the changing data as close together as possible
        for (auto i = 0; i < 16; i++) {
            unsigned long currentCount = mStreamOutCounts[i];
            if (currentCount > 0) {
                auto currentTime = std::chrono::steady_clock::now();

                // get the deltas
                unsigned long deltaCount = currentCount - mStreamOutLastCounts[i];
                mStreamOutLastCounts[i] = currentCount;
                auto deltaTime = currentTime - mStreamOutLastTimes[i];
                mStreamOutLastTimes[i] = currentTime;

                std::chrono::duration<float, std::micro> elapsedDuration = deltaTime;
                float elapsedSeconds = elapsedDuration.count()/1000000.0;

                std::string report = bandwidth_report(elapsedSeconds, deltaCount, deltaCount * mDataSize);
                report += " TOTALS: ";

                elapsedDuration =  currentTime - mStreamOutStartTimes[i];
                elapsedSeconds = elapsedDuration.count()/1000000.0;

                report += bandwidth_report(elapsedSeconds, currentCount, currentCount * mDataSize);

                fprintf(stderr,"******************* Sending bandwidth report *******************\n");
                BenchmarkMessage::Ptr message= std::make_shared<BenchmarkMessage>(BenchmarkMessage::REPORT, report);
                send(message, options);
            }
        }
    }
}

void
BenchmarkComputation::logSpeedThread(unsigned int threadIndex, unsigned int messageCount)
{
    ARRAS_LOG_ERROR("Message count = %u Thread index = %u\n", messageCount, threadIndex);
    for (auto i = 0; i < messageCount; i++) {
        fprintf(stderr,"%4u %4u Space: the final frontier. These are the voyages of the starship Enterprise.\n",
                threadIndex, i);
    }
}
Result BenchmarkComputation::configure(const std::string& op,
                                  ObjectConstRef& config)
{
    if (op == "start") {
        onStart();
        return Result::Success;
    } else if (op == "stop") {
        onStop();
        return Result::Success;
    } else if (op != "initialize") {
        return Result::Unknown;
    }  

    if (config["logThreads"].isIntegral()) {
        mLogThreadsCount = config["logThreads"].asInt();
    } else {
        mLogThreadsCount = 0;
    }
    if (config["logCount"].isIntegral()) {
        mLogCount = config["logCount"].asInt();
    } else {
        mLogCount = 0;
    }
    if (config["threads"].isIntegral()) {
        mConsumeCpuThreadCount = config["threads"].asInt();
    } else {
        mConsumeCpuThreadCount = 0;
    }
    if (config["allocateMb"].isIntegral()) {
        mConsumeMemoryAllocateMb = config["allocateMb"].asInt();
    } else {
        mConsumeMemoryAllocateMb = 0;
    }
    if (config["touchMb"].isIntegral()) {
        mConsumeMemoryTouchMb = config["touchMb"].asInt();
    } else {
        mConsumeMemoryTouchMb = 0;
    }
    if (config["touchOnce"].isIntegral()) {
        mConsumeMemoryTouchOnce = (config["touchOnce"].asInt() != 0);
    } else {
        mConsumeMemoryTouchOnce = 0;
    }
    if (config["sleepInOnStop"].isIntegral()) {
        mSleepInOnStop = config["sleepInOnStop"].asInt();
    } else {
        mSleepInOnStop = 0;
    }

    api::Object name = environment(EnvNames::computationName);
    if (name.isString())
        mName = name.asString();

    return Result::Success;
}

void
BenchmarkComputation::onStart()
{
    if (mConsumeCpuThreadCount > 0) {
        startConsumeCpu(mConsumeCpuThreadCount);
    }
    if (mConsumeMemoryAllocateMb > 0) {
        startConsumeMemory();
    }
}

void
BenchmarkComputation::onStop()
{
    if (mSleepInOnStop > 0) {
        sleep(mSleepInOnStop);
    }
    stopConsumeCpu();
    stopConsumeMemory();
    stopStreamOut();
}

void
BenchmarkComputation::logSpeedTest(unsigned int threadCount, unsigned int messageCount)
{
    std::vector<std::thread> threads;
    for (int i=0; i < threadCount; i++) {
        ARRAS_LOG_ERROR("Starting thread %d\n", i);
        threads.push_back(std::thread(&BenchmarkComputation::logSpeedThread, this, i, messageCount/threadCount));
    }
    for (size_t i=0; i<threads.size(); i++) {
        ARRAS_LOG_ERROR("Joining thread %d\n", i);
        if (threads[i].joinable()) threads[i].join();
    }
}

void reportVar(std::ostringstream& reportOut, const char var[])
{
    char* ptr = getenv(var);
    if (ptr != nullptr) {
        reportOut << var << "=" << ptr << "\n";
    } else {
        reportOut << var << " not set\n";
    }
}
    
Result
BenchmarkComputation::onMessage(const Message& message)
{
    if (message.classId() == BenchmarkMessage::ID) {
        BenchmarkMessage::ConstPtr tm = message.contentAs<BenchmarkMessage>();

        if (tm->mType == BenchmarkMessage::SEND_ACK) {
            api::Object options;
            options[api::MessageOptions::routingName] = std::string("For_") + tm->mFrom;
            BenchmarkMessage::Ptr response = std::make_shared<BenchmarkMessage>(BenchmarkMessage::ACK, "", mName);
            send(response, options);
        } else if (tm->mType == BenchmarkMessage::ACK) {
            if (tm->mFrom == "client") {
                mCreditArray[1].increment();
            } else {
                int index = std::stol(tm->mFrom.substr(9));
                if (index < 16) mCreditArray[index].increment();
            }
        } else if (tm->mType == BenchmarkMessage::START_STREAM_OUT) {
            startStreamOut(tm->mValue);
        } else if (tm->mType == BenchmarkMessage::SEND_REPORT) {
            reportStreamOut(tm->mFrom);
        } else if (tm->mType == BenchmarkMessage::STOP) {
            // stop everything
            stopStreamOut();
            stopConsumeCpu();
            stopConsumeMemory();
        } else if (tm->mType == BenchmarkMessage::PRINTENV) {
            api::Object options;
            options[api::MessageOptions::routingName] = std::string("For_") + tm->mFrom;
      
            std::ostringstream reportOut;
            int index = 0;
            while (environ[index] != nullptr) {
                BenchmarkMessage::Ptr message= std::make_shared<BenchmarkMessage>(BenchmarkMessage::REPORT, environ[index]);
                send(message, options);
                index++;
            }

            BenchmarkMessage::Ptr response = std::make_shared<BenchmarkMessage>(BenchmarkMessage::ACK, "logging done", mName);
            send(response, options);
        } else if (tm->mType == BenchmarkMessage::LOGSPEED) {
            // send a bunch of messages to stderr
            auto startTime = std::chrono::steady_clock::now();
            logSpeedTest(mLogThreadsCount, mLogCount);
            auto endTime = std::chrono::steady_clock::now();

            // generate speed report
            auto deltaTime = endTime - startTime;
            std::chrono::duration<float, std::micro> elapsedDuration = deltaTime;
            float elapsedSeconds = elapsedDuration.count()/1000000.0;
            std::ostringstream reportOut;
            unsigned int messages = mLogCount / mLogThreadsCount * mLogThreadsCount;
            reportOut << "Time: ";
            reportOut << std::fixed << std::setprecision(3) << elapsedSeconds;
            reportOut << "s Msgs: ";
            reportOut << messages;
            reportOut << " Rate: ";
            reportOut << std::fixed << std::setprecision(2) << (messages / elapsedSeconds);
            reportOut << "msg/s (";
            reportOut << std::fixed << std::setprecision(2) << 1.0/(messages / elapsedSeconds)*1000000.0;
            reportOut << "µs)\n";

            api::Object options;
            options[api::MessageOptions::routingName] = std::string("For_") + tm->mFrom;
      
            BenchmarkMessage::Ptr message= std::make_shared<BenchmarkMessage>(BenchmarkMessage::REPORT, reportOut.str());
            send(message, options);
            BenchmarkMessage::Ptr response = std::make_shared<BenchmarkMessage>(BenchmarkMessage::ACK, "logging done", mName);
            send(response, options);
        }

        return Result::Success;
    } else {
        ARRAS_LOG_DEBUG("Not handling unknown message class %s",message.describe().c_str());
        return Result::Unknown;
    }
}

void BenchmarkComputation::onIdle()
{
}

} // end namespace benchmark
} // end namespace arras4

