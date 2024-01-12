// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_INTERNAL_ERROR_H__
#define __ARRAS4_INTERNAL_ERROR_H__

#include <exception>
#include <stddef.h>
#include <string>

namespace arras4 {
    namespace impl {

// thrown by:
//    message_impl/Arras4MessageWriter : passed incorrect Message subclass
//    message_impl/SourceBuffer :invalid read without/outside buffer
//    routing/Addresser : passed incorrect Message subclass

class InternalError : public std::exception
{
public:
    InternalError(const std::string& aDetail)
        : mMsg(aDetail) {}
    ~InternalError() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

}
}
#endif
