// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "platform.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>

#include "InetSocketPeer.h"
#include "SocketPeer.h"
#include "Encryption.h"
#include "InvalidParameterError.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

#include <linux/un.h>

namespace arras4 {
    namespace network {

InetSocketPeer::InetSocketPeer() :
    SocketPeer(),
    mIPv4(0),
    mPort(0),
    mLocalPort(0)
{
}

InetSocketPeer::InetSocketPeer(unsigned short aPort, int aMaxPendingConnections) :
    SocketPeer(),
    mIPv4(0),
    mPort(0),
    mLocalPort(0)
{
    listen(aPort, aMaxPendingConnections);
}

InetSocketPeer::InetSocketPeer(const std::string& aHostname, unsigned short aPort) :
    SocketPeer(),
    mIPv4(0),
    mPort(0),
    mLocalPort(0)
{
    connect(aHostname, aPort);
}

InetSocketPeer::~InetSocketPeer()
{
}

//
// set up socket for accepting incoming connections
//
void
InetSocketPeer::listen(unsigned short aPort, int aMaxPendingConnections)
{
    if (mSocket != ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("InetSocketPeer already has an assigned socket");
    }

    // if this option is set, we are listening on a local UNIX domain socket
    mSocket = ::socket(AF_INET, SOCK_STREAM | ARRAS_SOCK_CLOEXEC, 0);
    if (mSocket == ARRAS_INVALID_SOCKET) {
        // save errno before doing anything else
	int save_errno = getSocketError();
	
        // throw an exception
        std::string err("Could not create listening socket: ");

        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    int yes = 1;
    if (::setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) == -1) {
        // this shouldn't get an error unless the operating system drops support

        // save errno before doing anything else
	int save_errno = getSocketError();
	
        // clean up
        shutdown();

        // throw an exception
        std::string err("Could not set reuse-address option on listening socket: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    // disable Nagle
    if (setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes)) < 0) {
        // this shouldn't get an error unless the operating system drops support

        // save errno before doing anything else
	int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::string err("Could not disable Nagle on listening socket: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    if (ioctl(mSocket, FIONBIO, (char*)&yes) < 0) {

        // this shouldn't get an error unless the operating system drops support

        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::string err("Could not set socket to non-blocking mode: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(aPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(mSocket, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::string err("Could not bind to socket for listening: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err.c_str());
    }

    if (aPort == 0) {
        socklen_t addrSize = sizeof(addr);
        if (getsockname(mSocket, reinterpret_cast<struct sockaddr*>(&addr), &addrSize) != 0) {
            // there really isn't an error that can happen here which isn't programmer
            // error but check just to be thorough

            // save errno before doing anything else
            int save_errno = getSocketError();

            // clean up
            shutdown();

            // throw an exception
            std::string err("Could not get port number");
            err += getErrorString(save_errno);
            throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
        }
        mLocalPort = ntohs(addr.sin_port);
    } else {
        mLocalPort = aPort;
    }

    if (::listen(mSocket, aMaxPendingConnections) != 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::string err("Failed to listen on socket: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    mIsListening = true;
}

void
InetSocketPeer::connect(const std::string& aHostname, unsigned short aPort)
{
    if (mSocket != ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("InetSocketPeer already has a socket assigned");
    }
    if (aPort == 0) {
        throw InvalidParameterError("InetSocketPeer cannot connect to a 0 port");
    }
    if (aHostname.empty()) {
        throw InvalidParameterError("InetSocketPeer cannot connect empty hostname");
    }

    // gethostbyname needs a socket to do the lookup so lookup the hostname before creating our own
    // socket to reduce the peek socket requirements
    // gethostbyname isn't very specific about it's errors so allocate and free a socket before 
    // calling gethostbyname to notice file descriptor exhaustion with a more useful message. gethostbyname
    // could still fail with file descriptor exhaustion in a multi-threaded environment but that will
    // be pretty rare.
    mSocket = socket(AF_INET, SOCK_STREAM | ARRAS_SOCK_CLOEXEC, 0);
    if (mSocket == ARRAS_INVALID_SOCKET) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // throw an exception
        std::string err("Could not create socket for connect: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    close(mSocket);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(aPort);

    mSocket = ARRAS_INVALID_SOCKET;
    int retries = 0;
    do {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo *result = NULL;
        int status = getaddrinfo(aHostname.c_str(), NULL, &hints, &result);
        if (status == 0) {
            /* success - grab the ip address and exit the loop */
            sockaddr_in* resultaddr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
            addr.sin_addr= resultaddr->sin_addr;
            break;
        } else {
            /* error */

            if ((status == EAI_AGAIN) || (retries++ > 5)) {

                // clean up
                shutdown();

                // throw an exception
                std::string err("Could not find hostname '");
                err += aHostname;
                err += "': ";
                throw PeerException(0, getCodeFromGetAddrInfoError(status), err);
            }
        }
    } while (1);

    mSocket = socket(AF_INET, SOCK_STREAM | ARRAS_SOCK_CLOEXEC, 0);
    if (mSocket == ARRAS_INVALID_SOCKET) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // throw an exception
        std::string err("Could not create socket for connect: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }
    enableKeepAlive();

    int yes = 1;

    // disable Nagle
    if (setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes)) < 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        std::string err("Could not disable Nagle on connecting socket: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    if (::connect(mSocket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::string err("Could not connect to remote endpoint: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    mIPv4 = ntohl(addr.sin_addr.s_addr);
    mPort = aPort;

    socklen_t addrSize = sizeof(addr);
    if (getsockname(mSocket, reinterpret_cast<struct sockaddr*>(&addr), &addrSize) < 0) {
        // there really isn't an error that can happen here which isn't programmer
        // error but check just to be thorough

        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::string err("Could not get port number");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }
    mLocalPort = ntohs(addr.sin_port);
}

} // end namespace network
} // end namespace arras

