// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_IOCAPTURE_H__
#define __ARRAS4_IOCAPTURE_H__

#include <mutex>
#include <string>

namespace arras4 {
    namespace impl {

class IoCapture
{
public:
    virtual void onStdout(const char* buf,unsigned count) = 0;
    virtual void onStderr(const char* buf,unsigned count) = 0;
    virtual ~IoCapture() {}
};
 
class SimpleIoCapture : public IoCapture
{
public:
    void clear(); 
    void onStdout(const char* buf,unsigned count);
    void onStderr(const char* buf,unsigned count);
    std::string out() const;
    std::string err() const;

private:
    mutable std::mutex mMutex;
    std::string mOut;
    std::string mErr;
};

}
}
#endif
