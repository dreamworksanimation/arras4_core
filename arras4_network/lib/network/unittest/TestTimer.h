// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_TESTTIMER_H_
#define __ARRAS_TESTTIMER_H_

#include <chrono>
#include <stdexcept>

const std::chrono::time_point<std::chrono::steady_clock> EPOCH;

class TestTimer
{
public:
    TestTimer()
        : mElapsedTime(0) {};


    void start() 
    {
        mStartTrackingTime = std::chrono::steady_clock::now();
    };
    
    
    void stop()
    {
        if (mStartTrackingTime == EPOCH) {
            throw std::logic_error("Error TestTimer::stop() called before TestTimer::start()");
        }

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = end - mStartTrackingTime;
        mElapsedTime = elapsed.count();
    };
    
    
    /// this returns the # of seconds that have elapsed, namely the # of seconds
    /// that have elapsed between the last start() and stop() calls
    double timeElapsed() const 
    { 
        return mElapsedTime; 
    };

    double now() const 
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> epoch = now.time_since_epoch();
        return epoch.count();
    };

private:
    std::chrono::time_point<std::chrono::steady_clock> mStartTrackingTime;
    double mElapsedTime;
};

#endif //__ARRAS_TESTTIMER_H_

