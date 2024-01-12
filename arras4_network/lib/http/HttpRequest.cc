// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "HttpRequest.h"
#include "HttpException.h"

#include <network/Buffer.h>
#include <network/MultiBuffer.h>

#include <curl/curl.h>

namespace {

const char* sMethods[] = {
    "GET",
    "POST",
    "PUT",
    "DELETE",
    "OPTIONS",
    "PUT"
};

const char* sContentType[] = {
    "text/plain",
    "text/xml",
    "text/html",
    "application/json",
    "application/x-www-form-urlencoded",
    "application/octet-stream",
    "image/png"
};

// handles sending data to the peer (POST or PUT methods)
size_t
read_callback(void* aBuffer, size_t aSize, size_t aItems, void* aUser)
{
    // read at most aSize*aItems from our input buffer into aBuffer; 
    // aUser is a pointer to a Buffer instance
    arras4::network::Buffer* buf = static_cast<arras4::network::Buffer*>(aUser);
    return buf->read(static_cast<unsigned char*>(aBuffer), aSize * aItems);
}

int
seek_callback(void* aUser, curl_off_t offset, int /* origin */)
{
    // move read pointer to given offset; 
    // aUser is a pointer to a Buffer instance
    arras4::network::Buffer* buf = static_cast<arras4::network::Buffer*>(aUser);
    buf->seek(offset);
    return 0;
}

// handles data returned from the peer
size_t
write_callback(void* aBuffer, size_t aSize, size_t aItems, void* aUser)
{
    // write at most aSize*aItems from aBuffer into our output buffer; 
    // aUser is a pointer to a MultiBuffer instance, since total size is
    // unknown
    arras4::network::MultiBuffer* mbuf = static_cast<arras4::network::MultiBuffer*>(aUser);
    if (aSize > 0 && aItems > 0) {
        return mbuf->write(reinterpret_cast<unsigned char*>(aBuffer), aSize * aItems);
    }

    return 0;
}

size_t
header_callback(char* aBuffer, size_t aSize, size_t aItems, void* aUser)
{
    // aUser is a pointer to an HttpResponse object
    arras4::network::HttpResponse* resp = static_cast<arras4::network::HttpResponse*>(aUser);
    if (resp) {
        std::string headerLine;
        headerLine.assign(aBuffer, aSize*aItems);

        // ignore empty lines
        if (!headerLine.empty()) {
            // find the header delimiter (:)
            std::string::size_type pos = headerLine.find_first_of(':');

            // ignore anything without a delimiter (:), for example, status lines
            if (pos != std::string::npos) {
                std::string key(headerLine.substr(0, pos));

                // find first non-space character after the delimiter (:)
                std::string::size_type pos2 = headerLine.find_first_not_of(' ', pos+1);
                // strip off any trailing \r\n characters
                std::string::size_type pos3 = headerLine.find_first_of("\r\n", pos+1);

                std::string value;
                if (pos2 != std::string::npos)
                    value = headerLine.substr(pos2, pos3-pos2);
                else
                    value = headerLine.substr(pos+1);

                resp->addHeader(key, value);
            }
        }

        // indicate to Curl that we processed it, even if it was empty
        return aItems * aSize;
    }

    return 0;
}

std::string
escapeString(const std::string& input, CURL* curl) {
    char *output = curl_easy_escape(curl, input.c_str(), (int)input.size());
    if(!output) {
        throw arras4::network::HttpException("Can not escape input argument");
    }
    std::string ret(output);
    curl_free(output);
    return ret;
}

} // namespace

namespace arras4 {
    namespace network {

class ScopedCurlGlobalInit 
{
public:
    ScopedCurlGlobalInit();
    ~ScopedCurlGlobalInit();
    operator bool() const;

    ScopedCurlGlobalInit& operator=(const ScopedCurlGlobalInit&) = delete;
private:
    bool mInitError;
};

ScopedCurlGlobalInit::ScopedCurlGlobalInit()
{
    auto result = curl_global_init(CURL_GLOBAL_ALL);
    mInitError = result != 0;
}

ScopedCurlGlobalInit::~ScopedCurlGlobalInit()
{
    curl_global_cleanup();
}

ScopedCurlGlobalInit::operator bool() const
{
    return !mInitError;
}


HttpRequest::HttpRequest(const std::string& aUrl, HttpMethod aMethod)
    : mUrl(aUrl)
    , mUserAgent("Arras Curl")
    , mMethod(aMethod)
    , mContentType(TEXT_PLAIN)
    , mVerifyServerCert(true)
{
    // C++11 ensures that this static is intialized only once, even if multiple threads are
    // constructing this object at once
    // https://en.wikipedia.org/wiki/Double-checked_locking
    static ScopedCurlGlobalInit sInitCurl = ScopedCurlGlobalInit();
    if (!sInitCurl) {
        throw HttpException("Unable to initialize Curl");
    }
    // Global destructors will be called at program exit (or DLL unload)
    mCurl = curl_easy_init();
}


void
HttpRequest::cleanup()
{
    mHeaders.clear();
    mParams.clear();
    mContentType = TEXT_PLAIN;
}

HttpRequest::~HttpRequest()
{
    curl_easy_cleanup(mCurl);
}

std::string
HttpRequest::getParamString() const
{
    std::string url;
    if (mParams.empty()) {
        url = mUrl;
    } else {
        auto it = mParams.begin();
        std::string data = "?" + escapeString(it->first, mCurl) + "=" + escapeString(it->second, mCurl);
        ++it;

        for(; it != mParams.end(); ++it) {
            data += "&"+ escapeString(it->first, mCurl) + "=" + escapeString(it->second, mCurl);
        }
        url =  mUrl + data;
    }
    return url;
}


const HttpResponse&
HttpRequest::submit(const void* aData, int aLen, int timeout)
{
    mResponse.reset();

    CURL* curl = mCurl;
    curl_easy_reset(curl);

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    const std::string& url = getParamString();

    // set the URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
   
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, sMethods[mMethod]);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

    // accept invalid TLS certs if the user wants
    if (mVerifyServerCert) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1);
    }
    else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    }

    // add the User-Agent header
    mHeaders.insert(Headers::value_type("User-Agent", mUserAgent));

    BufferUniquePtr sendBuf;
  
    if (mMethod == PUT_MULTIPART) {
        throw HttpException("multipart PUT is not supported");
    }
    
    if ((mMethod == POST || mMethod == PUT)) {

        if (!aData)
            throw HttpException("POST/PUT without data");

        // if the user supplied their own, Content-Type, let that override our internal cached version
        const std::string& contentType = mMethod != PUT_MULTIPART ? sContentType[mContentType] : "multipart/form-data";
        mHeaders.insert(Headers::value_type(HTTP_CONTENT_TYPE, contentType));
        
        // There is no "ConstBuffer" type, but the read callback doesn't modify
        // the buffer contents.
        unsigned char* dataPtr = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(aData));
        sendBuf = BufferUniquePtr(new Buffer(dataPtr,aLen,aLen)); // C++14: std::make_unique

        if (mMethod == POST) {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
        } else if (mMethod == PUT) {
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        }
        curl_easy_setopt(curl, CURLOPT_SEEKFUNCTION, seek_callback);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, aLen);
        curl_easy_setopt(curl, CURLOPT_READDATA, sendBuf.get());
        curl_easy_setopt(curl, CURLOPT_SEEKDATA, sendBuf.get());

      
    }

    // append headers 
    curl_slist* curlHeaders = nullptr;
    std::vector<std::string> headers(mHeaders.size());
    std::vector<std::string>::iterator it = headers.begin();

    for (const auto& pr : mHeaders) {
        *it = pr.first + ": " + pr.second;
        curlHeaders = curl_slist_append(curlHeaders, (*it).c_str());
        ++it;
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlHeaders);

    // read any return data into mResponse
    MultiBufferUniquePtr responseBuf(new MultiBuffer());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, responseBuf.get());

    // capture all response headers
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &mResponse);

    // do the submit
    CURLcode res = curl_easy_perform(curl);

    if (CURLE_OK != res) {
        throw HttpException(curl_easy_strerror(res));
    }

    // if data has any length, collect it into a single buffer in mResponse
    if (responseBuf->bytesWritten()) {
        Buffer* buf = mResponse.allocResponseData(responseBuf->bytesWritten());
        responseBuf->collect(*buf);
    } 

    long code = 0L;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    mResponse.setResponseStatus((ResponseCode)code, std::string());

    // Clean up
    curl_slist_free_all(curlHeaders);
    return mResponse;
}

const HttpResponse&
HttpRequest::submit(const void* aData, int aLen)
{
    return submit(aData, aLen, 0 /* don't timeout*/);
}

const HttpResponse& 
HttpRequest::submit(const std::string& stringData)
{
    return submit(stringData.c_str(), (int)(stringData.length()+1), 0 /* don't timeout */);
}

const HttpResponse& 
HttpRequest::submit(const std::string& stringData, int timeout)
{
    return submit(stringData.c_str(), (int)(stringData.length()+1), timeout);
}

} 
} 

