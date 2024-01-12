// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "HttpResponse.h"
#include <network/Buffer.h>
#include <network/InvalidParameterError.h>

#include <sstream>

namespace arras4 {
    namespace network {

HttpResponse::HttpResponse() : mResponseCode(HTTP_OK) 
{
}

const std::string&
HttpResponse::header(const std::string& aHeader) const
{
    Headers::const_iterator h = mHeaders.find(aHeader);
    if (h == mHeaders.end()) {
        std::stringstream ss;
        ss << "Header '" << aHeader << "' not found in response";
        std::string msg(ss.str());
        throw InvalidParameterError(msg.c_str());
    }

    return h->second;
}

void
HttpResponse::reset()
{
    mResponseData.reset();
    mResponseCode = HTTP_OK;
    mResponseDesc = "";
    mHeaders.clear();
}

Buffer*
HttpResponse::allocResponseData(size_t size)
{
    mResponseData = std::make_shared<Buffer>(size);
    return mResponseData.get();
}

void
HttpResponse::setResponseStatus(ResponseCode aCode, const std::string& aDesc)
{
    mResponseCode = aCode;
    mResponseDesc = aDesc;
}
bool
HttpResponse::getResponseString(std::string& out) const
{
    if (!mResponseData)
        return false;
    else 
        mResponseData->copyIntoString(out);
       
    return true;
}

void
HttpResponse::addHeader(const std::string& aHeader, const std::string& aValue)
{
    mHeaders[aHeader] = aValue;
}

} // namespace network
} // namespace arras

