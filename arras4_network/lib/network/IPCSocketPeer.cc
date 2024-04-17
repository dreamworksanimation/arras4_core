// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "network_platform.h"

#ifdef PLATFORM_WINDOWS

#include <winsock2.h>
#include <BaseTsd.h>
#include <WS2tcpip.h>
#include "mstcpip.h"
typedef SSIZE_T ssize_t;

#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>

#endif

#include "IPCSocketPeer.h"
#include "SocketPeer.h"
#include "Encryption.h"
#include "InvalidParameterError.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

#ifdef PLATFORM_LINUX
    #include <linux/un.h>
#elif defined PLATFORM_APPLE
    #include <sys/un.h>
#endif

namespace arras4 {
namespace network {

IPCSocketPeer::IPCSocketPeer()
{
}

IPCSocketPeer::~IPCSocketPeer()
{
    unlink(mSocketName.c_str());
}

void
IPCSocketPeer::listen(const std::string& aIPCName, int aMaxPendingConnections)
{
    if (mSocket != ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("SocketPeer already in use");
    }
    if (aMaxPendingConnections < 1) {
        throw InvalidParameterError("Max pending connections must be a positive number");
    }
    if (aIPCName.empty()) {
        throw InvalidParameterError("IPC name is empty");
    }

    // Linux allows the path to be up to UNIX_PATH_MAX
    // but we're restricting it to UNIX_PATH_MAX-1 so that
    // NULL termination can be left intact.
    if (aIPCName.length() > (UNIX_PATH_MAX - 1)) {
        throw InvalidParameterError("IPC name too long. Must be 107 chars or fewer.");
    }

    // create an IPC socket
    // the most likely cause of an error here would be running out of file descriptors
    SOCKET_CREATE(mSocket, AF_UNIX);
    
    if (mSocket == ARRAS_INVALID_SOCKET) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // throw an exception
        std::string err("Could not create IPC socket for listening: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }
    enableKeepAlive();

    // make the I/O non-blocking
    int yes = 1;

#ifdef PLATFORM_WINDOWS
    if (ioctlsocket(mSocket, FIONBIO, (u_long*)&yes) < 0) {
#else
    if (ioctl(mSocket, FIONBIO, (char*)&yes) < 0) {
#endif
        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::stringstream ss;
        ss << "Could not set socket '" << aIPCName << "' to non-blocking mode";
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), ss.str());
    }

    // create the sockaddr for UNIX
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    // the name size was checked to fit in UNIX_PATH_MAX above
    strcpy(addr.sun_path, aIPCName.c_str());
    size_t len = sizeof(addr.sun_family) + strlen(addr.sun_path);

    if (::bind(mSocket, reinterpret_cast<struct sockaddr*>(&addr), (socklen_t)len) < 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::stringstream ss;
        ss << "Could not bind to socket file '" << aIPCName << "'";
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), ss.str());
    }

    // remember the socket name for removal in the destructor
    mSocketName = aIPCName;

    if (::listen(mSocket, aMaxPendingConnections) < 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::string err("Failed to listen on socket: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    // flag this peer as listening
    mIsListening = true;
}

void
IPCSocketPeer::connect(const std::string& aIPCName)
{
    if (mSocket != ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("IPCSocketPeer already has a socket assigned");
    }
    if (aIPCName.empty()) {
        throw InvalidParameterError("IPC name is empty");
    }

    // Linux allows the path to be up to UNIX_PATH_MAX
    // but we're restricting it to UNIX_PATH_MAX-1 so that
    // NULL termination can be left intact.
    if (aIPCName.length() > (UNIX_PATH_MAX - 1)) {
        throw InvalidParameterError("IPC name too long. Must be 107 chars or fewer.");
    }

    SOCKET_CREATE(mSocket, AF_UNIX);
    if (mSocket == ARRAS_INVALID_SOCKET) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // throw an exception
        std::string err("Could not create socket for IPC connection: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }
    enableKeepAlive();

    // create the sockaddr for UNIX
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    // the name size was checked to fit in UNIX_PATH_MAX above
    strcpy(addr.sun_path, aIPCName.c_str());
    size_t len = sizeof(addr.sun_family) + strlen(addr.sun_path);

    if (::connect(mSocket, (const sockaddr*)&addr, (socklen_t)len) < 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        SOCKET_CLOSE(mSocket);

        mSocket = ARRAS_INVALID_SOCKET;
        std::string err("Could not connect to IPC endpoint: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }
}

} // end namespace network
} // end namespace arras4

