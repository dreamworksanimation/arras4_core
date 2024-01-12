// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_TEST_COMPUTATIONH__
#define __ARRAS4_TEST_COMPUTATIONH__

#include <computation_api/Computation.h>
#include <chrono>
#include <thread>

namespace arras4 {
    namespace test {

class TestComputation : public arras4::api::Computation
{
public:
    TestComputation(arras4::api::ComputationEnvironment* env) :
        arras4::api::Computation(env) {}

    arras4::api::Result onMessage(const arras4::api::Message& message);
    
    void onIdle();

    arras4::api::Result configure(const std::string& op,
                                  arras4::api::ObjectConstRef& config);
    void workerThread();
private:
    std::string mName;
    bool mForward = false;
    bool mReply = false;
    std::string mErrorInOnMessage;
    std::string mErrorInThread;
    std::string mErrorInStart;
    std::string mErrorInStop;
    int mSleepInOnMessage = 0;
    int mSleepInOnIdle = 0;
    std::chrono::steady_clock::time_point mLastIdleLog;
    std::thread mWorkerThread;
};

}
}
#endif
