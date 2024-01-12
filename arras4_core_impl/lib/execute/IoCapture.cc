// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "IoCapture.h"

namespace arras4 {
    namespace impl {

void SimpleIoCapture::clear()
{ 
    std::unique_lock<std::mutex> lock(mMutex);
    mOut.clear();
    mErr.clear();
}

void SimpleIoCapture::onStdout(const char* buf,unsigned count) 
{ 
    std::unique_lock<std::mutex> lock(mMutex);
    mOut += std::string(buf,count); 
}
    
void SimpleIoCapture::onStderr(const char* buf,unsigned count) 
{ 
    std::unique_lock<std::mutex> lock(mMutex);
    mErr += std::string(buf,count); 
}
    
std::string SimpleIoCapture::out() const
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mOut;
}

std::string SimpleIoCapture::err() const
{
    std::unique_lock<std::mutex> lock(mMutex);
    return mErr;
}

}
}
