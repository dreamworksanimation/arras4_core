// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_HTTPSERVERRESPONSE_H__
#define __ARRAS4_HTTPSERVERRESPONSE_H__

#include "httpserver_types.h"
#include <microhttpd.h>

struct MHD_Connection;

namespace arras4 {
    namespace network {

class HttpServerResponse
{
public:
    HttpServerResponse(struct MHD_Connection* aConnection) :
        mConnection(aConnection),
        mData(nullptr),
        mDataLen(0),
        mContentType("text/plain"),
        mCode(200)
        {}

    ~HttpServerResponse() { delete[] mData; }

    void setContentType(const std::string& aType) { mContentType = aType; }
    void setResponseCode(ServerResponseCode aCode) { mCode = aCode; }
    ServerResponseCode responseCode() const { return mCode; }
    void setResponseText(const std::string& aText) { mText = aText; }
    const std::string& responseText() const { return mText; }
    
    void write(const void* aData, int aLen);
    void write(const std::string& aStringData);
    
private:
    friend class HttpServer;
    
    struct MHD_Connection* mConnection;
    unsigned char* mData;
    int mDataLen;

    std::string mContentType;

    // used for sending non-200 return codes
    std::string mText;
    ServerResponseCode mCode;

    MHD_Result queue();
};

}
} 

#endif

