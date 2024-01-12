// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_COMPUTATION_LOAD_ERROR_H__
#define __ARRAS4_COMPUTATION_LOAD_ERROR_H__

#include <exception>
#include <stddef.h>
#include <string>

namespace arras4 {
    namespace impl {

class ComputationLoadError : public std::exception
{
public:
    ComputationLoadError(const std::string& aDetail)
        : mMsg(aDetail) {}
    ~ComputationLoadError() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

}
}
#endif
