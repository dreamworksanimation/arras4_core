// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0


#include "TestComputation.h"
#include <message/TestMessage.h>

#include <computation_api/standard_names.h>

#include <arras4_log/Logger.h>

#include <signal.h>   // for sigaction()
#include <string.h>
#include <unistd.h>   // for sleep()

using namespace arras4::api;

namespace arras4 {
    namespace test {

COMPUTATION_CREATOR(TestComputation);

namespace {
// a signal handler that does nothing so it is ignored
void
sigtermHandler(int /*aSignal*/, siginfo_t* /*aSigInfo*/, void* /*aCtx*/)
{
    write(0, "In signal handler\n", strlen("In signal handler\n"));
}

} // and anonymous namespace

Result TestComputation::configure(const std::string& op,
                                  ObjectConstRef& config)
{
    if (op == "start") {
        if (mErrorInStart == "throw") {
            ARRAS_LOG_ERROR("Throwing runtime_exception for testing, in 'configure(start)'");

            throw std::runtime_error("Thrown for testing in 'configure(start)'");
        } else if (mErrorInStart == "segfault") {
            ARRAS_LOG_ERROR("Deliberately generating segfault for testing, in 'configure(start)'");
            int* ptr= nullptr;
            // cppcheck-suppress nullPointer
            *ptr = *ptr + 1;
        } else if (mErrorInStart == "invalid") {
            ARRAS_LOG_ERROR("Returning 'invalid message' error for testing, in 'configure(start' ");
            return Result::Invalid;
        } else if (mErrorInStart == "abort") {
            ARRAS_LOG_ERROR("Deliberately aborting in 'configure(start)'");
            abort();
        } else if (mErrorInStart == "exit") {
            ARRAS_LOG_ERROR("Deliberately exiting with status 0 in 'configure(start)'");
            exit(0);
        } else if (mErrorInStart == "exit10") {
            ARRAS_LOG_ERROR("Deliberately exiting with status 10 in 'configure(start)'");
            exit(10);
        } else if (mErrorInStart == "sigterm") {
            ARRAS_LOG_ERROR("Deliberately signalling SIGTERM in 'configure(start)'");
            kill(getpid(), SIGTERM);
        } else if (mErrorInStart == "sigkill") {
            ARRAS_LOG_ERROR("Deliberately signalling SIGKILL in 'configure(start)'");
            kill(getpid(), SIGKILL);
        } else if (!mErrorInStart.empty()){
            ARRAS_LOG_WARN("Unknown config value for 'errorInOnMessage': '%s'. Supported values are 'throw','segfault','invalid'",mErrorInOnMessage.c_str());
        }
        mLastIdleLog = std::chrono::steady_clock::now();
        return Result::Success;
    } else if (op == "stop") {
        if (mErrorInStop == "throw") {
            ARRAS_LOG_ERROR("Throwing runtime_exception for testing, in 'configure(stop)'");
            throw std::runtime_error("Thrown for testing in 'configure(stop)'");
        } else if (mErrorInStop == "segfault") {
            ARRAS_LOG_ERROR("Deliberately generating segfault for testing, in 'configure(stop)'");
            int* ptr= nullptr;
            // cppcheck-suppress nullPointer
            *ptr = *ptr + 1;
        } else if (mErrorInStop == "invalid") {
            ARRAS_LOG_ERROR("Returning 'invalid message' error for testing, in 'configure(stop)' ");
            return Result::Invalid;
        } else if (mErrorInStop == "abort") {
            ARRAS_LOG_ERROR("Deliberately aborting in 'configure(stop)'");
            abort();
        } else if (mErrorInStop == "exit") {
            ARRAS_LOG_ERROR("Deliberately exiting with status 0 in 'configure(stop)'");
            exit(0);
        } else if (mErrorInStop == "exit10") {
            ARRAS_LOG_ERROR("Deliberately exiting with status 10 in 'configure(stop)'");
            exit(10);
        } else if (mErrorInStop == "sigterm") {
            ARRAS_LOG_ERROR("Deliberately signalling SIGTERM in 'configure(stop)'");
            kill(getpid(), SIGTERM);
        } else if (mErrorInStart == "sigkill") {
            ARRAS_LOG_ERROR("Deliberately signalling SIGKILL in 'configure(stop)'");
            kill(getpid(), SIGKILL);
        } else if (!mErrorInStop.empty()){
            ARRAS_LOG_WARN("Unknown config value for 'errorInStop' '%s'",mErrorInOnMessage.c_str());
        }
        return Result::Success;
    } else if (op != "initialize") {
        return Result::Unknown;
    }  
 
    if (config["forward"].isBool()) {
        mForward = config["forward"].asBool();
    } 
    if (config["reply"].isBool()) {
        mReply = config["reply"].asBool();
    }
    if (config["sleepInConfig"].isIntegral()) {
        int sleepSecs = config["sleepInConfig"].asInt();
        ARRAS_LOG_DEBUG("Sleeping in config for %d seconds...",sleepSecs);
        sleep(sleepSecs);
        ARRAS_LOG_DEBUG("...continuing after sleep");
    }
    if (config["errorInConfig"].isString()) {
        std::string err = config["errorInConfig"].asString();
        if (err == "throw") {
            ARRAS_LOG_ERROR("Throwing runtime_exception for testing, in 'configure'");
            throw std::runtime_error("Thrown for testing in 'configure'");
        } else if (err == "segfault") {
            ARRAS_LOG_ERROR("Deliberately generating segfault for testing, in 'configure'");
            int* ptr= nullptr;
            // cppcheck-suppress nullPointer
            *ptr = *ptr + 1;
        } else if (err == "invalid") {
            ARRAS_LOG_ERROR("Returning 'invalid configuration' error for testing, in 'configure'");
            return Result::Invalid;
        } else {
            ARRAS_LOG_WARN("Unknown config value for 'errorInConfig': '%s'. Supported values are 'throw','segfault','invalid'",err.c_str());
        }
    }
    if (config["ignoreSigterm"].isIntegral() && (config["ignoreSigterm"].asInt() > 0)) {
        struct sigaction action, oldAction;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_SIGINFO;
        action.sa_sigaction = sigtermHandler;
        ARRAS_LOG_WARN("Ignoring SIGTERM");
        sigaction(SIGINT, &action, &oldAction);
        sigaction(SIGTERM, &action, &oldAction);
    }
    if (config["errorInStart"].isString()) {
        mErrorInStart = config["errorInStart"].asString();
    }
    if (config["errorInStop"].isString()) {
        mErrorInStop= config["errorInStop"].asString();
    }
    if (config["errorInOnMessage"].isString()) {
        mErrorInOnMessage = config["errorInOnMessage"].asString();
    }
    if (config["errorInThread"].isString()) {
        mErrorInThread= config["errorInThread"].asString();
    }
    if (config["sleepInOnMessage"].isIntegral()) {
        mSleepInOnMessage = config["sleepInOnMessage"].asInt();
    }
    if (config["sleepInOnIdle"].isIntegral()) {
        mSleepInOnIdle = config["sleepInOnIdle"].asInt();
    }
    Object name = environment(EnvNames::computationName);
    if (name.isString())
        mName = name.asString();

    return Result::Success;
}

void
TestComputation::workerThread()
{
    if (mErrorInThread == "throw") {
        ARRAS_LOG_ERROR("Throwing runtime_exception for testing, in 'workerThread'");
        throw std::runtime_error("Thrown for testing in 'workerThread'");
    }
}

struct foobar {
};
    
Result
TestComputation::onMessage(const Message& message)
{
    if (mErrorInOnMessage == "throw") {
        ARRAS_LOG_ERROR("Throwing runtime_exception for testing, in 'onMessage'");
        throw std::runtime_error("Thrown for testing in 'onMessage'");
    } else if (mErrorInOnMessage == "segfault") {
        ARRAS_LOG_ERROR("Deliberately generating segfault for testing, in 'onMessage'");
        int* ptr= nullptr;
        // cppcheck-suppress nullPointer
        *ptr = *ptr + 1;
    } else if (mErrorInOnMessage == "invalid") {
        ARRAS_LOG_ERROR("Returning 'invalid message' error for testing, in 'onMessage' : message is : %s",message.describe().c_str());
        return Result::Invalid;
    } else if (mErrorInOnMessage == "abort") {
        ARRAS_LOG_ERROR("Deliberately aborting in 'onMessage'");
        abort();
    } else if (mErrorInOnMessage == "exit") {
        ARRAS_LOG_ERROR("Deliberately exiting with status 0 in 'onMessage'");
        exit(0);
    } else if (mErrorInOnMessage == "exit10") {
        ARRAS_LOG_ERROR("Deliberately exiting with status 10 in 'onMessage'");
        exit(10);
    } else if (mErrorInOnMessage == "sigterm") {
        ARRAS_LOG_ERROR("Deliberately signalling SIGTERM in 'onMessage'");
        kill(getpid(), SIGTERM);
    } else if (mErrorInOnMessage == "sigkill") {
        ARRAS_LOG_ERROR("Deliberately signalling SIGKILL in 'onMessage'");
        kill(getpid(), SIGKILL);
    } else if (!mErrorInOnMessage.empty()){
        ARRAS_LOG_WARN("Unknown config value for 'errorInOnMessage': '%s'. Supported values are 'throw','segfault','invalid'",mErrorInOnMessage.c_str());
    }
    if (mSleepInOnMessage) {
        ARRAS_LOG_DEBUG("Sleeping in onMessage for %d seconds...",mSleepInOnMessage);
        sleep(mSleepInOnMessage);
        ARRAS_LOG_DEBUG("...continuing after sleep");
    }
    if (message.classId() == TestMessage::ID) {
        TestMessage::ConstPtr tm = message.contentAs<TestMessage>();
        ARRAS_LOG_DEBUG_STR(mName << " received: " << tm->describe());
    
        if (mForward) {
            TestMessage::Ptr forward = std::make_shared<TestMessage>(*tm);
            forward->text() += " [forward";
            if (!mName.empty())
                forward->text() += " from " + mName;
            forward->text() += "]";
            ARRAS_LOG_DEBUG_STR(mName << " forwarding: " << forward->describe());
            send(forward, withSource(message));
        }  
        if (mReply) {
            TestMessage::Ptr reply = std::make_shared<TestMessage>(*tm);
            reply->text() += " [reply";
            if (!mName.empty())
                reply->text() += " from " + mName;
            reply->text() += "]";
            ARRAS_LOG_DEBUG_STR(mName << " replying: " << reply->describe());
            ObjectConstRef fromAddr = message.get("from");
            send(reply, sendTo(fromAddr));
        }
        if (!mErrorInThread.empty()) {
            mWorkerThread = std::thread(&TestComputation::workerThread, this);
            mWorkerThread.join();
        }

        return Result::Success;
    } else {
        ARRAS_LOG_DEBUG("Not handling unknown message class %s",message.describe().c_str());
        return Result::Unknown;
    }
}

void TestComputation::onIdle()
{
    std::chrono::steady_clock::duration d = 
        std::chrono::steady_clock::now() - mLastIdleLog;
    if (d > std::chrono::seconds(5)) {
        ARRAS_LOG_DEBUG("...TestComputation idle...");
         mLastIdleLog = std::chrono::steady_clock::now();
    }
    if (mSleepInOnIdle) {
        ARRAS_LOG_DEBUG("Sleeping in onIdle for %d seconds...",mSleepInOnIdle);
        sleep(mSleepInOnIdle);
        ARRAS_LOG_DEBUG("...continuing after sleep");
    }
}

}
}

