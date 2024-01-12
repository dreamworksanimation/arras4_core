// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_HTTPSERVERREQUEST_H__
#define __ARRAS4_HTTPSERVERREQUEST_H__

#include "httpserver_types.h"

struct MHD_Connection;

namespace arras4 {
    namespace network {

class HttpServerRequest
{
public:
    HttpServerRequest(struct MHD_Connection* aConnection, 
                      const std::string& aUrl) : 
        mConnection(aConnection),
        mUrl(aUrl) 
        {}
    ~HttpServerRequest() {  delete [] mData; }

    const std::string& url() const { return mUrl; }
    const unsigned char* data() const { return mData; }
    unsigned int dataLen() const { return mDataLen; }
    bool getDataString(std::string& out) const;
    const StringMap& headers() const { return mHeaders; }
    const StringMap& queryParams() const { return mQueryParams; }
    bool header(const std::string& aHeaderName,std::string& out) const;
    bool queryParam(const std::string& aParamName,std::string& out) const;

    // get connection client address
    bool getClientAddress(std::string& out) const;

    // internal use only
    void _setHeader(const std::string& name, const std::string& value)
        { mHeaders.insert(StringMap::value_type(name,value)); }
    void _setQueryParam(const std::string& name, const std::string& value)
        { mQueryParams.insert(StringMap::value_type(name,value)); }  
    void _setData(const unsigned char* aData, unsigned int aDataLen);
    void _appendData(const unsigned char* aData, unsigned int aOffset, unsigned int aDataLen);
    unsigned char* _allocateData(unsigned int aDataLen);



private:
    struct MHD_Connection* mConnection;
    std::string mUrl;

    StringMap mHeaders;
    StringMap mQueryParams;
    unsigned char* mData = nullptr;
    unsigned int mDataLen = 0;
};

} 
}

#endif
