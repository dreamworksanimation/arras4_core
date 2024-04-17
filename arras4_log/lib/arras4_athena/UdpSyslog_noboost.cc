// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "UdpSyslog_noboost.h"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

#include <iostream>

#ifdef PLATFORM_WINDOWS
#include <winsock2.h>
#include <WS2tcpip.h>

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#endif

namespace {
    const int MAX_GETADDR_RETRIES = 5;
    const int MAX_SENDTO_RETRIES = 5;
}

namespace arras4 {
namespace log {

UdpSyslog::UdpSyslog(const std::string& hostname, unsigned short port)
{
#ifdef PLATFORM_WINDOWS
    // make sure WinSock is initialized
    // Request at least Winsock 2.0
    WORD wVersionRequested = MAKEWORD(2, 0);
    WSADATA wsaData;
    int err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    throw std::runtime_error("Failed to initialize winsock");
#endif

    // get local host name
    char buf[1024];
    int status = ::gethostname(buf, 1024);
    if (status == 0)
    mLocalHostName = buf;
    else
    mLocalHostName = "Unknown";

    // create socket
    mSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (mSocket == ARRAS_INVALID_SOCKET)
        throw std::runtime_error("Failed to create socket for UDP syslog");

    // fill in target address
    std::memset(&mServerAddress, 0, sizeof(mServerAddress));
    mServerAddress.sin_family = AF_INET;
    mServerAddress.sin_port = htons(port);

    // look up hostname
    unsigned retries = 0;
    do {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        struct addrinfo* result = nullptr;
        status = getaddrinfo(hostname.c_str(), NULL, &hints, &result);
        if (status == 0) {
            // success - grab the ip address and exit the loop
            sockaddr_in* resultaddr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
            mServerAddress.sin_addr = resultaddr->sin_addr;
            break;
        }
        else {
            if ((status == EAI_AGAIN) || (retries++ > MAX_GETADDR_RETRIES)) {
                throw std::runtime_error("Couldn't find host " + hostname + " for UDP syslog");
            }
        }
    } while (1);
}

UdpSyslog::~UdpSyslog()
{
    if (mSocket != ARRAS_INVALID_SOCKET)
        SOCKET_CLOSE(mSocket);
#ifdef PLATFORM_WINDOWS
    WSACleanup();
#endif
}

void UdpSyslog::sendMessage(int priority, const tm* timeStamp, const std::string& ident, const std::string& message)
{
    static const char months[12][4] =
    {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    // format from RFC 3164 : BSD syslog Protocol
    std::stringstream ss;
    ss << "<" << priority << +">"
        << months[timeStamp->tm_mon] << " "
        << std::setfill(' ')
        << std::setw(2) << timeStamp->tm_mday << " "
        << std::setfill('0')
        << std::setw(2) << timeStamp->tm_hour << ":"
        << std::setw(2) << timeStamp->tm_min << ":"
        << std::setw(2) << timeStamp->tm_sec << " "
        << mLocalHostName << " "
        << ident << " ";

    // truncate super long messages
    // The message size limit for datagrams is 64K (65507 chars without packet header)
    // Using 65000 to allow room for log header
    size_t size = message.size();
    if (size > 65000) {
        ss << message.substr(0, 65000) << " (Truncated from " << size << " chars)";
    }
    else {
        ss << message;
    }

    std::string packet = ss.str();

    // sendto() can rarely but randomly return EPERM. Just retry and in the
    // extremely unlikely case of not working on retries then just drop it
    for (int i = 0; i < MAX_SENDTO_RETRIES; i++) {
#ifdef PLATFORM_WINDOWS
        int sent =
#else
        ssize_t sent =
#endif
            ::sendto(mSocket, packet.data(), packet.size(), 0, (sockaddr*)&mServerAddress, sizeof(mServerAddress));
        if (sent > 0) break;
    }
}

}
}
