// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PROGRESS_SENDER_H__
#define __ARRAS4_PROGRESS_SENDER_H__

#include <message_api/Object.h>
#include <shared_impl/ThreadsafeQueue.h>
#include <execute/ProcessManager.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace arras4 {
    namespace client {

class ProgressSender
{
public:
    ProgressSender(impl::ProcessManager& procman) 
	: mQueue("progress"), mProcessManager(procman)
    {}
    ~ProgressSender();

    bool isDisabled() { return mAddress.empty(); }

    void setAutoExecCmd(const std::string& cmd);
    void setChannel(const std::string& channel);

    void progress(api::ObjectConstRef message);

private:
    void startSending();
    void stopSending();
    void sendProc();
    void sendWait(unsigned seconds);
    bool doAutoExec();

    std::string mAutoExecCmd;  // empty means no autoexec
    std::string mAddress;      // empty means progress gui is disabled

    impl::ThreadsafeQueue<api::Object> mQueue;

    std::atomic<bool> mRun{true};
    std::mutex mThreadMutex;
    std::mutex mStopMutex;
    std::condition_variable mStopCondition;

    bool mIsSending{false};
    std::thread mSendThread;

    impl::ProcessManager& mProcessManager; 
};

}
}
#endif
