// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGE_FORMAT_ERROR_H__
#define __ARRAS4_MESSAGE_FORMAT_ERROR_H__

#include <exception>
#include <stddef.h>
#include <string>

namespace arras4 {
    namespace api {

// This exception is thrown when message data is found to be corrupt,
// unreadable, or otherwise wrong. Generally it indicates a coding, linking
// or version-matching error somewhere. User code may throw this during
// serialize/deserialize methods if it detects a problem.
class MessageFormatError : public std::exception
{
public:
    MessageFormatError(const std::string& aDetail)
        : mMsg(aDetail) {}
    ~MessageFormatError() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

}
}
#endif
