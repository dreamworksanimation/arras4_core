// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "HttpServerResponse.h"
#include "HttpServerException.h"

#include <microhttpd.h>
#include <cstring>

#define HTTP_CONTENT_TYPE "Content-Type"
#define HTTP_CONTENT_LENGTH "Content-Length"

namespace arras4 {
namespace network {

void
HttpServerResponse::write(const void* aData, int aLen)
{
    if (aLen != mDataLen) {
        delete [] mData;
        mData = new unsigned char[aLen];
        mDataLen = aLen; 
    }
    memcpy(mData,aData,aLen);
}

void
HttpServerResponse::write(const std::string& aStringData)
{
    write(aStringData.data(),(int)aStringData.length());
}


MHD_Result
HttpServerResponse::queue()
{
    struct MHD_Response* response = nullptr;

    if (mCode == 200 && mDataLen) {

        // normal, successful response

        response = MHD_create_response_from_buffer(mDataLen, mData, MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(response, HTTP_CONTENT_TYPE, mContentType.c_str());
        const std::string lenStr = std::to_string(mDataLen);
        MHD_add_response_header(response, HTTP_CONTENT_LENGTH, lenStr.c_str());
      
    } else {
        // abnormal, unsuccessful response

        // the user may have supplied some custom response text, use that if available
        if (!mText.empty()) {
            response = MHD_create_response_from_buffer(mText.length(), 
                                                       const_cast<char*>(mText.c_str()), 
                                                       MHD_RESPMEM_MUST_COPY);
        } else {
            std::string text;

            switch (mCode) {
            case 405: // METHOD_NOT_ALLOWED
                text = "Method not supported";
                break;
            case 404: // NOT_FOUND
                text = "Resource not found";
                break;
            default:
                text = "Unknown server error";
                break;
            }

            response = MHD_create_response_from_buffer(text.length(), 
                                                       const_cast<char*>(text.c_str()), 
                                                       MHD_RESPMEM_MUST_COPY);
        }

        // headers
        MHD_add_response_header(response, HTTP_CONTENT_TYPE, "text/plain");
    }

    if (response) {
        if (MHD_YES != MHD_queue_response(mConnection, mCode, response)) {
            throw HttpServerException("HttpServerResponse Could not queue response");
        }

        MHD_destroy_response(response);
        return MHD_YES;
    } else {
        return MHD_NO;
    }
}

} 
}
