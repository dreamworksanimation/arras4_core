// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CONFIGURATION_ERROR_H__
#define __ARRAS4_CONFIGURATION_ERROR_H__

#include <exception>
#include <stddef.h>
#include <string>

namespace arras4 {
    namespace impl {

class ConfigurationError : public std::exception
{
public:
    ConfigurationLoadError(const std::string& aDetail)
        : mMsg(aDetail) {}
    ~ConfigurationError() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

}
}
#endif
