// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef CREDITS_INCLUDED
#define CREDITS_INCLUDED

#include <condition_variable>
#include <thread>
#include <mutex>

//
//    Class for keeping track of communications credits so that only a given
//    number of messages cam be sent without acknowledgement. The critic
//    function is waitAndDecrement(). By allowing a thread to wait and
//    be immediately notified when there is an available credit no unnecessary
//    delay is introduced yet no unnecessary CPU is consumed. During construction
//    or any time before begining of use the number of credits should be set.
//    Each time an acknowledge is received increment)() should be called.
//    Any time another message needs to be sent waitAndDecrement() should be
//    called. It will block until it's possible to decrement it.
//
//    In this particular configuration it is very much like semaphores.
//

class Credits {
  public:

    Credits(int initialValue=5) : mValue(initialValue) {};

    //
    // set the value if it has changed and notify the waiters to wake up
    //
    void set(int newValue) {
        std::unique_lock<std::mutex> lock(mMutex);
        if (mValue != newValue) {
            mValue = newValue;
            mCondition.notify_all();
        }
    }

    //
    // increment the value
    //
    void increment() {
        std::unique_lock<std::mutex> lock(mMutex);
        mValue++;
        mCondition.notify_all();
    }

    //
    // decrement the value
    // 
    void decrement() {
        std::unique_lock<std::mutex> lock(mMutex);
        mValue--;
        mCondition.notify_all();
    }

    //
    // wait and decrement the value
    // 
    void waitAndDecrement(int minimum=0) {
        std::unique_lock<std::mutex> lock(mMutex);
        while (mValue <= minimum) {
            mCondition.wait(lock);
        }
        mValue--;
        mCondition.notify_all();
    }

  private:
    std::mutex mMutex;
    std::condition_variable mCondition;
    int mValue;
};

#endif // CREDITS_INCLUDED
