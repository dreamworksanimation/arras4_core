// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_THREADSAFE_QUEUE_IMPLH__
#define __ARRAS4_THREADSAFE_QUEUE_IMPLH__

#include "ThreadsafeQueue.h"
#include <exceptions/ShutdownException.h>

namespace arras4 {
    namespace impl {

template<typename T>
void ThreadsafeQueue<T>::push(const T& t)
{
    std::unique_lock<std::mutex> lock(mMutex);
    if (mShutdown) {
        throw ShutdownException("Queue was shut down");
    }
    mQueue.push(t);
    lock.unlock();
    mNotEmptyCondition.notify_one();
}

template<typename T>
bool ThreadsafeQueue<T>::pop(T& t,
                          const std::chrono::microseconds& timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    if (mShutdown) {
        throw ShutdownException("Queue was shut down");
    }
    while (mQueue.empty()) {
        if (timeout == std::chrono::microseconds::zero()) {
            mNotEmptyCondition.wait(lock);
        } else {
            std::cv_status cvs = mNotEmptyCondition.wait_for(lock,timeout);
            if (cvs == std::cv_status::timeout)
                return false;
        }
        if (mShutdown) {
            throw ShutdownException("Queue was shut down");
        }
    }
    t = mQueue.front();
    mQueue.pop();
    if (mQueue.empty())
        mEmptyCondition.notify_all();
    return true;
}

template<typename T>
bool ThreadsafeQueue<T>::waitUntilEmpty(const std::chrono::microseconds& timeout)
{
    std::unique_lock<std::mutex> lock(mMutex);
    if (mShutdown) {
        throw ShutdownException("Queue was shut down");
    }
    while (!mQueue.empty()) {
        if (timeout == std::chrono::microseconds::zero()) {
            mEmptyCondition.wait(lock);
        } else {
            std::cv_status cvs = mEmptyCondition.wait_for(lock,timeout);
            if (cvs == std::cv_status::timeout)
                return false;
        }
        if (mShutdown) {
            throw ShutdownException("Queue was shut down");
        }
    }
    return true;
}

template<typename T>
void ThreadsafeQueue<T>::shutdown()
{   
    std::unique_lock<std::mutex> lock(mMutex);
    mShutdown = true;
    lock.unlock();
    mNotEmptyCondition.notify_all();
    mEmptyCondition.notify_all();
}

}
}
#endif
