// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Encryption.h"

namespace arras4{
namespace network {

EncryptException::EncryptException(const char* aMsg)
    : mMsg(aMsg)
{

}

EncryptException::~EncryptException() throw()
{

}

const char*
EncryptException::what() const throw()
{
    return mMsg.c_str();
}

EncryptContext::~EncryptContext()
{

}

EncryptState::~EncryptState()
{

}

} // namespace network
} // namespace arras4


