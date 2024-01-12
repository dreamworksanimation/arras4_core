// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_SHUTDOWN_EXCEPTION_H__
#define __ARRAS4_SHUTDOWN_EXCEPTION_H__

#include <exception>
#include <stddef.h>
#include <string>

namespace arras4 {
    namespace impl {

// thrown by a data source when it has been shut down
// and you try to read/write from it. Operations
// in progress at the time of shutdown may also
// throw this exception

class ShutdownException : public std::exception
{
public:
    ShutdownException(const std::string& detail)
        : mMsg(detail) {}
  
    ~ShutdownException() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

}
}
#endif
