// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0


#ifndef __ARRAS4_UDP_SYSLOG_NO_BOOST_H__
#define __ARRAS4_UDP_SYSLOG_NO_BOOST_H__

#include "athena_platform.h"

#include <string>
#include <time.h>


#ifdef PLATFORM_WINDOWS
#define NOMINMAX 1
#include <winsock2.h>

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#endif

// warning : don't include <syslog.h> here : it makes
// some of the Logger::Level enum values unusable...

namespace arras4 {
    namespace log {

        // Sends log messages to syslog service (typically port 514) using
        // UDP
        class UdpSyslog
        {
        public:

            // Will send messages to given host name (e.g. localhost) and port (e.g. 514)
            UdpSyslog(const std::string& hostname, unsigned short port);
            ~UdpSyslog();

            // send message with given priority (e.g. LOG_LOCAL0 | LOG_DEBUG), time, ident and message 
            void sendMessage(int priority, const tm* timeStamp, const std::string& ident, const std::string& message);

        private:

            ARRAS_SOCKET mSocket;
            sockaddr_in mServerAddress;
            std::string mLocalHostName;
        };

    }
}
#endif
