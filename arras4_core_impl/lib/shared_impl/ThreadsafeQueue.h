// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_THREADSAFE_QUEUEH__
#define __ARRAS4_THREADSAFE_QUEUEH__

#include <mutex>
#include <condition_variable>
#include <queue>

namespace arras4 {
    namespace impl {

// simple mutexed implementation of a concurrent queue
template<typename T>
class ThreadsafeQueue
{
public:
    ThreadsafeQueue(const std::string& label="Queue") : 
        mLabel(label),
        mShutdown(false) {}
    ~ThreadsafeQueue() { shutdown(); }

    void push(const T& t);
 
    // pop waits for a a maximum period of 'timeout'
    // for an item to be available on the queue for popping.
    // It returns true if an item was available before the timeout
    // and was placed in 't', or false if the timeout expired
    // and 't' was left unchanged..
    //
    // passing a timeout of zero blocks indefinitely until an 
    // item is available, and therefore always returns true
    bool pop(T& env,
             const std::chrono::microseconds& timeout =
             std::chrono::microseconds::zero());

    // blocks until the next time the queue is empty, the
    // timeout has expired or shutdown is called. Returns
    // true if terminated because queue was empty.
    bool waitUntilEmpty(const std::chrono::microseconds& timeout =
                        std::chrono::microseconds::zero());

    // when shutdown is called, waiting push and pop
    // calls will unblock and throw ShutdownException 
    // soon after the shutdown call.
    // future calls to push/pop will immediately throw
    // the exception
    void shutdown();

    

private:
    std::queue<T> mQueue;
    std::mutex mMutex;
    std::condition_variable mEmptyCondition;
    std::condition_variable mNotEmptyCondition;
    std::string mLabel; // helps debugging
    bool mShutdown;
};

}
}
#endif
