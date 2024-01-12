// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_HTTPRESPONSE_H__
#define __ARRAS4_HTTPRESPONSE_H__

#include "http_types.h"

#include <network/Buffer.h>
#include <memory>
#include <string>

namespace arras4 {
    namespace network {

class Buffer;

class HttpResponse
{
public:
    HttpResponse(); 

    size_t numHeaders() const { return mHeaders.size(); }
    const std::string& header(const std::string& aHeader) const;
    const Headers& headers() const { return mHeaders; }
    ResponseCode responseCode() const { return mResponseCode; }
    const std::string& responseDesc() const { return mResponseDesc; }
    Buffer* responseData() { return mResponseData.get(); }    
    bool getResponseString(std::string& out) const;
    
    void reset();
    void setResponseStatus(ResponseCode aCode, const std::string& aDesc);
    void addHeader(const std::string& aHeader, const std::string& aValue);
    Buffer* allocResponseData(size_t size);
    
protected:

    Headers mHeaders;
    std::shared_ptr<Buffer> mResponseData;         
    ResponseCode mResponseCode;
    std::string mResponseDesc;
   
   
};

} 
}

#endif 

