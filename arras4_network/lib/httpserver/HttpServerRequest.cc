// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "HttpServerRequest.h"
#include <algorithm>
#include <cstring>

#include <microhttpd.h>  // for MHD_get_connection_info
#include <sys/socket.h>
#include <netdb.h>       // for getnameinfo


namespace arras4 {
namespace network {

int constexpr BUFSIZE = 1024;

bool
HttpServerRequest::header(const std::string& aHeaderName, std::string& out) const
{
    auto it = mHeaders.find(aHeaderName);
    if (it != mHeaders.end()) {
        out = it->second;
        return true;
    }
    return false;
}

bool
HttpServerRequest::queryParam(const std::string& aParamName,std::string& out) const
{
    auto it = mQueryParams.find(aParamName);
    if (it != mQueryParams.end()) {
        out = it->second;
        return true;
    }
    return false;
}

bool 
HttpServerRequest::getDataString(std::string& out) const
{
    if (mData) {
        out = std::string((const char*)mData,mDataLen);
        return true;
    } else {
        return false;
    }
}

bool
HttpServerRequest::getClientAddress(std::string& out) const
{
    if (mConnection) {
        struct sockaddr *addr;
        addr = MHD_get_connection_info(mConnection, MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
        if (addr) {
            char buf[BUFSIZE]; // buffer for client address, don't need port
            if (getnameinfo(addr, 16, buf, sizeof(buf), 0, 0, NI_NUMERICHOST) == 0) {
                out = std::string(buf);
                return true;
            }
        }
    }
    return false;  // unable to get client address
}

void
HttpServerRequest::_setData(const unsigned char* aData, unsigned int aDataLen)
{
    _allocateData(aDataLen);
    memcpy(mData, aData, aDataLen);
}

void
HttpServerRequest::_appendData(const unsigned char* aData, unsigned int aOffset, unsigned int aDataLen)
{
    unsigned int nBytes = std::min(mDataLen-aOffset, aDataLen);
    memcpy(mData+aOffset, aData, nBytes);
}

unsigned char*
HttpServerRequest::_allocateData(unsigned int aDataLen)
{
    if (aDataLen != mDataLen) delete [] mData;

    mData = new unsigned char[aDataLen];
    mDataLen = aDataLen;

    return mData;
}

} 
} 

