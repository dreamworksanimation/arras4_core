// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_HTTP_REQUESTH__
#define __ARRAS4_HTTP_REQUESTH__

#include "http_types.h"
#include "HttpResponse.h"

#include <string>

typedef void CURL;

namespace arras4 {
    namespace network {

class HttpRequest
{

public:
  
    HttpRequest(const std::string& aUrl, 
                HttpMethod aMethod=GET);
    ~HttpRequest();

    void addParam(const std::string& aKey, const std::string& aValue)
        { mParams.emplace(std::make_pair(aKey, aValue)); }
    void addHeader(const std::string& aHeader, const std::string& aValue)
        { mHeaders[aHeader] = aValue; }
    void setContentType(HttpContentType aType)
        { mContentType = aType; }
    void setUserAgent(const std::string& aUserAgent)
        { mUserAgent = aUserAgent; }
  
    void setVerifyServerCert(bool aVerify)
        { mVerifyServerCert = aVerify; }
    void setMethod(HttpMethod aMethod) { mMethod = aMethod; }
    void setUrl(const std::string& aUrl) { mUrl = aUrl; }
    std::string getUrl() const { return getParamString(); }   
    HttpMethod getMethod() const { return mMethod; }    
    const Headers& getHeaders() const { return mHeaders; }

    // For operations without data (GET, DELETE, etc).
    const HttpResponse& submit() { return submit(nullptr,0,0); }
    const HttpResponse& submit(int timeout) { return submit(nullptr,0,timeout); }

    // For operations with data (POST, PUT, etc). Throws
    //      InvalidParameterException (malformed URL, usually),
    //      PeerException (any network connection issues)
    const HttpResponse& submit(const void* aData, int aLen);
    const HttpResponse& submit(const void* aData, int aLen, const int timeout);
    const HttpResponse& submit(const std::string& stringData);
    const HttpResponse& submit(const std::string& stringData, const int timeout);

    void cleanup(); 

private:
    std::string getParamString() const;

    std::string mUrl;
    std::string mUserAgent;
    HttpResponse mResponse;
    HttpMethod mMethod;
    HttpContentType mContentType;
    Parameters mParams;
    Headers mHeaders;
    bool mVerifyServerCert;

    CURL* mCurl;
};

} 
} 

#endif 
