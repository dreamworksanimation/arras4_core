// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_HTTPSERVER_EXCEPTION_H__
#define __ARRAS4_HTTPSERVER_EXCEPTION_H__

#include <exception>
#include <stddef.h>
#include <string>

namespace arras4 {
    namespace network {

class HttpServerException : public std::exception
{
public:
    HttpServerException(const std::string& aDetail)
        : mMsg(aDetail) {}
    ~HttpServerException() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

}
}
#endif
