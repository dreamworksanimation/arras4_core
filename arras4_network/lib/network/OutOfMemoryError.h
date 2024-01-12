// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_OUTOFMEMORY_ERROR_H__
#define __ARRAS4_OUTOFMEMORY_ERROR_H__

#include <exception>
#include <stddef.h>
#include <string>

namespace arras4 {
    namespace network {

class OutOfMemoryError : public std::exception
{
public:
    OutOfMemoryError(const std::string& aDetail)
        : mMsg(aDetail) {}
    ~OutOfMemoryError() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

}
}
#endif
