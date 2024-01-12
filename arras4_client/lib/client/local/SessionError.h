// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_SESSION_ERROR_H__
#define __ARRAS4_SESSION_ERROR_H__

#include <exception>

namespace arras4 {
namespace client {

class SessionError : public std::exception
{
public:
    SessionError(const std::string& aDetail) : mDetail(aDetail) {}
     ~SessionError() noexcept override {}
    const char* what() const noexcept override { return mDetail.c_str(); }
protected:
    std::string mDetail;
};

}
}
#endif
