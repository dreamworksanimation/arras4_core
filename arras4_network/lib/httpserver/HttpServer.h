// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_HTTPSERVER_H__
#define __ARRAS_HTTPSERVER_H__

#include "HttpRequestEvent.h"

#include <network/SocketPeer.h>
#include <string>
#include <thread>

#include <microhttpd.h>
// struct MHD_Daemon;
// struct MHD_Connection;

namespace arras4 {
    namespace network {

        constexpr unsigned int DEFAULT_THREAD_POOL_SIZE=4;

        class HttpServer
        {
        public:
            HttpServer(unsigned short aListenPort, unsigned int aThreadPoolSize=DEFAULT_THREAD_POOL_SIZE);
            HttpServer(unsigned short aListenPort, const std::string& aTLSCertFile, const std::string& aTLSKeyFile, 
                       unsigned int aThreadPoolSize=DEFAULT_THREAD_POOL_SIZE);
            ~HttpServer();

            HttpRequestEvent GET;
            HttpRequestEvent POST;
            HttpRequestEvent PUT;
            HttpRequestEvent DELETE;
            HttpRequestEvent OPTIONS;

            int getListenPort() { return mPort; }
            ARRAS_SOCKET createListenSocket(unsigned short& aListenPort);

            // internal-use only
            void _prepare(HttpServerRequest&, struct MHD_Connection*);
            MHD_Result _complete(HttpServerResponse&);

        private:
            MHD_Daemon* mDaemon;
            unsigned short mPort;
        };
    }
}

#endif // __ARRAS_HTTPSERVER_H__

