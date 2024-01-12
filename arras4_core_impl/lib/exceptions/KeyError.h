// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_KEY_ERROR_H__
#define __ARRAS4_KEY_ERROR_H__

#include <exception>
#include <stddef.h>
#include <string>

// thrown by:
//   routing/ComputationMap : UUID or name not found in map

namespace arras4 {
    namespace impl {

class KeyError : public std::exception
{
public:
    KeyError(const std::string& aDetail)
        : mMsg(aDetail) {}
    ~KeyError() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

}
}
#endif
