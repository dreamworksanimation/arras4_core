// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "HttpServer.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpServerException.h"

#include <sys/socket.h>
#include <netinet/in.h>

#include <fstream> // ifstream to read ssl cert files
#include <sstream>
#include <string.h> // strlen
#include <stdexcept> // logic_error (aka The Unhappy Spock Error)
#include <sys/stat.h>
#include <sys/types.h>

// microhttpd docs recomend including this after basic types
// have been defined
#include <microhttpd.h>

#define HTTP_CONTENT_TYPE "Content-Type"

namespace {

using arras4::network::HttpServer;
using arras4::network::HttpServerResponse;
using arras4::network::HttpServerRequest;

const std::string s404 = "Not Found";
const std::string s405 = "Method Not Allowed";
const std::string s500 = "Internal Server Error: ";

struct ResponseState
{
    ResponseState(struct MHD_Connection* aConnection, const char* aUrl);
    ~ResponseState() { delete mResp; delete mReq; }

    HttpServerResponse* mResp = nullptr;
    HttpServerRequest* mReq = nullptr;
    // pointer to current insert point in multiple-chunk data upload
    unsigned int mOffset = 0;
    // total upload size (from Content-Length header)
    unsigned int mContentLength = 0;
    // phase==1 is allocating-storage/appending-data, phase==2 is finishing up
    int mPhase = 0;
};

ResponseState::ResponseState(struct MHD_Connection* aConnection, const char* aUrl)
    : mResp(new HttpServerResponse(aConnection))
    , mReq(new HttpServerRequest(aConnection, aUrl))
    , mOffset(0)
    , mContentLength(0)
    , mPhase(1)
{
}

MHD_Result
handleError(struct MHD_Connection* aConnection,
            unsigned int statusCode,
            const std::string& responseMsg,
            enum MHD_ResponseMemoryMode mode)
{
    MHD_Result rtn = MHD_NO;
    struct MHD_Response* resp = MHD_create_response_from_buffer(responseMsg.size() + 1,
                                                                const_cast<char*>(responseMsg.c_str()),
                                                                mode);
    if (resp) {
        rtn = MHD_queue_response(aConnection, statusCode, resp);
        MHD_destroy_response(resp);
    }
    return rtn;
}

MHD_Result
dispatch(
    void* aData,
    struct MHD_Connection* aConnection,
    const char* aUrl,
    const char* aMethod,
    const char* /*aVersion*/,
    const char* aUploadData,
    size_t* aUploadDataSize, // in&out variable
    void** aPtr
)
{
    // The aPtr variable is for per-connection data.
    // This is used primarily for POST requests where the full contents
    // of the upload data are not immediately available.
    // New connections will always have *aPtr set to null.
    // On the first connection we create an instance of ResponseStates
    // on the heap and set *aPtr to it.
    //
    // So long as the return value of this function is MHD_YES, and
    // we have not made a call to MHD_queue_response() MicroHttpd will
    // continue calling this function with the same value of *aPtr.
    // Note: this->_complete() will make a call to MHD_queue_response().
    //
    // Upload data from POST and PUT requests are appended to
    // the mData buffer of the ResponseState's HttpServerResponse (mResp)
    // instance, and progress is tracked via ResponseState's mPhase attribute.
    //
    // The usage of the *aPtr keeps dispatch thread-safe as concurrent
    // requests will all have a unique instance of ResponseState on the heap.
    //
    // The MHD_OPTION_NOTIFY_COMPLETED callback `mhdCallback` is called after dispatch
    // has either completed successfully or encountered an error, at which
    // point the `mhdCallback` function will delete *aPtr and deallocate the memory.
    // See: https://www.gnu.org/software/libmicrohttpd/manual/libmicrohttpd.html#microhttpd_002dcb

    if (*aPtr == nullptr) {
        // make one of these regardless of request method...
        ResponseState* state = new ResponseState(aConnection, aUrl);
        *aPtr = state;
        return MHD_YES;
    }

    MHD_Result rtn = MHD_NO;
    try {        
        HttpServer* server = static_cast<HttpServer*>(aData);
        ResponseState* state = static_cast<ResponseState*>(*aPtr);
        HttpServerResponse* resp = state->mResp;
        HttpServerRequest* req = state->mReq;
        const std::string method(aMethod);

        if (state->mPhase == 1) {
            server->_prepare(*req, aConnection);
        }

        if (method=="GET") {
            server->GET(*req, *resp);
            rtn = server->_complete(*resp);

        }
        else if (method == "DELETE") {
            server->DELETE(*req, *resp);
            rtn = server->_complete(*resp);

        } else if (method == "POST" || method == "PUT") {
            switch (state->mPhase) {
            case 1: {
                   
                // handle message data
                if (aUploadDataSize && *aUploadDataSize) {
                    // we may get POST/PUT data in multiple chunks. Phase 1 includes allocation of request data storage, and
                    // appending to same; if the size of the request data in this chunk fills out the number of bytes described
                    // in Content-Length, then we can go straight to Phase 2 (send the response). If not, we need to keep a
                    // pointer to the current insert point in the data and process any further chunks in Phase 2.

                    if (state->mContentLength == 0 && state->mPhase == 1) {
                        std::string lenStr;
                        if (req->header("Content-Length",lenStr)) {
                            state->mContentLength = atoi(lenStr.c_str());
                            if (state->mContentLength) {
                                req->_allocateData(state->mContentLength + 1); // extra byte for null termination
                            }
                        }
                    }

                    const unsigned int usDataSize = static_cast<unsigned int>(*aUploadDataSize);
                    req->_appendData((const unsigned char*)aUploadData, state->mOffset, usDataSize);
                    state->mOffset += usDataSize;

                    if (state->mOffset >= state->mContentLength) {

                        // null terminate the request body
                        unsigned char zero = 0;
                        req->_appendData(&zero, state->mContentLength, 1);

                        // then time to invoke events and change phase
                        if (method == "POST") {
                            server->POST(*req, *resp);
                        } else { 
                            server->PUT(*req, *resp);
                        }
                        state->mPhase = 2;
                    }

                    *aUploadDataSize = 0;
                }
                else {

                    if (method == "POST") {
                        server->POST(*req, *resp);
                    } else {
                        server->PUT(*req, *resp);
                    }
                    
                    // if there was no POST message body, we have only the one phase, so send the response now
                    rtn = server->_complete(*resp);      
                }

                rtn = MHD_YES;
            } break;

            case 2: {
                   
                    // if there was data in the POST message, it's a two-phase operation, so send the response now
                    rtn = server->_complete(*resp);       
            } break;

            default: {
                throw std::logic_error("POST/PUT message sequence error");
            } break;
            }
        } else {
            rtn = handleError(aConnection, 405, s405, MHD_RESPMEM_PERSISTENT);
        }

        if (rtn == MHD_NO) {
            rtn = handleError(aConnection, 404, s404, MHD_RESPMEM_PERSISTENT);
        }
    } catch (std::exception& e) {
        const std::string error(e.what());
        const std::string responseMsg(s500 + error);
        rtn = handleError(aConnection, 500, responseMsg, MHD_RESPMEM_MUST_COPY);
    }

    return rtn;
}

void
mhdCallback(void* /*cls*/, struct MHD_Connection* /*conn*/, void** aPtr, MHD_RequestTerminationCode)
{
    if (*aPtr) { 
        ResponseState* state = (ResponseState*)*aPtr;
        delete state;
        *aPtr = nullptr;
    }
}

size_t
url_decode(void* /*cls*/, struct MHD_Connection* /*c*/, char *s)
{
    // return data in s (null-term), return strlen(s) from this function
    // since this is not used for POST data (form key/value pairs) and
    // we want each user of this server to handle URI-decoding themselves,
    // we will just return with s intact
    return strlen(s);
}

} // namespace

namespace arras4 {
namespace network {

HttpServer::HttpServer(unsigned short aListenPort, unsigned int aThreadPoolSize)
    : HttpServer(aListenPort, std::string(), std::string(), aThreadPoolSize)
{
}

ARRAS_SOCKET HttpServer::createListenSocket(unsigned short& aListenPort)
{

    ARRAS_SOCKET socket = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socket == ARRAS_INVALID_SOCKET) {
        throw HttpServerException("Couldn't create a socket for HTTP server");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(aListenPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(socket, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(socket);
        perror("bind");
        throw HttpServerException("Couldn't bind HTTP server socket");
    }

    if (::listen(socket, 32)) {
        close(socket);
        throw HttpServerException("Couldn't listen on HTTP server socket");
    }

    if (aListenPort == 0) {
        socklen_t addrSize = sizeof(addr);
        if (getsockname(socket, reinterpret_cast<struct sockaddr*>(&addr), &addrSize) != 0) {
            close(socket);
            throw HttpServerException("Couldn't get the port from the HTTP server socket");
        }
        aListenPort = ntohs(addr.sin_port);
    }

    return socket;
}


HttpServer::HttpServer(unsigned short aListenPort, 
                       const std::string& aTLSCertFile, 
                       const std::string& aTLSKeyFile,
                       unsigned int aThreadPoolSize)
    : mDaemon(0)
    , mPort(aListenPort)
{
    std::string certData;
    std::string keyData;

    if (!aTLSCertFile.empty() && !aTLSKeyFile.empty()) {
        // convenience...it gets long and ugly below without it
        typedef std::istreambuf_iterator<std::string::value_type> s_type;

        std::ifstream cert(aTLSCertFile.c_str());
        if (cert.is_open()) {
            certData = std::string(s_type(cert), s_type());

            std::ifstream key(aTLSKeyFile.c_str());
            if (key.is_open()) {
                keyData = std::string(s_type(key), s_type());
            }
        }
    }

    unsigned int flags = (MHD_USE_SELECT_INTERNALLY |
                          MHD_USE_EPOLL_LINUX_ONLY |
                          MHD_USE_EPOLL_TURBO);

    ARRAS_SOCKET socket = createListenSocket(mPort);

    if (!certData.empty() && !keyData.empty()) {
        flags |= MHD_USE_SSL;

        mDaemon = MHD_start_daemon(
                        flags,
                        mPort,
                        nullptr,
                        nullptr,
                        dispatch,
                        this,

                        MHD_OPTION_LISTEN_SOCKET,
                        socket,

                        MHD_OPTION_UNESCAPE_CALLBACK,
                        url_decode,
                        this,

                        MHD_OPTION_NOTIFY_COMPLETED,
                        mhdCallback,
                        this,
                        
                        MHD_OPTION_THREAD_POOL_SIZE,
                        aThreadPoolSize,

                        MHD_OPTION_HTTPS_MEM_CERT,
                        certData.c_str(),

                        MHD_OPTION_HTTPS_MEM_KEY,
                        keyData.c_str(),

                        MHD_OPTION_END
                    );

    } else {
        mDaemon = MHD_start_daemon(
                        flags,
                        mPort,
                        nullptr,   // apc
                        nullptr,   // apc_cls
                        dispatch,  // dh
                        this,      // dh_cls

                        MHD_OPTION_LISTEN_SOCKET,
                        socket,

                        MHD_OPTION_UNESCAPE_CALLBACK,
                        url_decode,
                        this,

                        MHD_OPTION_NOTIFY_COMPLETED,
                        mhdCallback,
                        this,

                        MHD_OPTION_THREAD_POOL_SIZE,
                        aThreadPoolSize,

                        MHD_OPTION_END
                    );

    }

    if (!mDaemon) {
        std::stringstream ss;
        ss << "Could not listen on port " << mPort;
        throw HttpServerException(ss.str());
    }
}

HttpServer::~HttpServer()
{
    if (mDaemon) MHD_stop_daemon(mDaemon);
}

MHD_Result
HttpServer::_complete(HttpServerResponse& aResp) {
    return aResp.queue();
}

static MHD_Result
key_value_iterator(void* aCls, enum MHD_ValueKind aKind, const char* aKey, const char* aVal)
{

    std::string value;
    if (aVal) {
        value = std::string(aVal);
    }
    HttpServerRequest* req = (HttpServerRequest*)aCls;

    if (aKind == MHD_HEADER_KIND) req->_setHeader(std::string(aKey), value);
    if (aKind == MHD_GET_ARGUMENT_KIND) req->_setQueryParam(std::string(aKey), value);

    return MHD_YES;
}

void
HttpServer::_prepare(HttpServerRequest& aReq, MHD_Connection* aConn)
{
    // headers...
    MHD_get_connection_values(aConn, MHD_HEADER_KIND, key_value_iterator, &aReq);

    // query params...
    MHD_get_connection_values(aConn, MHD_GET_ARGUMENT_KIND, key_value_iterator, &aReq);
}

} 
} 

