// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_NETWORK_PLATFORM_H__
#define __ARRAS4_NETWORK_PLATFORM_H__

#ifndef SOCKET_CLOSE

    #ifdef PLATFORM_WINDOWS

        typedef unsigned int ARRAS_SOCKET;
        const unsigned int ARRAS_INVALID_SOCKET = (~0);
        #define SOCKET_CLOSE(x) ::closesocket(x)
        #define SOCKET_CREATE(x,t) x = ::socket(t, SOCK_STREAM, 0);

    #elif defined PLATFORM_LINUX

        typedef int ARRAS_SOCKET;
        const int ARRAS_INVALID_SOCKET = -1;
        #define SOCKET_CLOSE(x) ::close(x)
        #define SOCKET_CREATE(x,t) x = ::socket(t, SOCK_STREAM | SOCK_CLOEXEC, 0);

    #elif defined PLATFORM_APPLE

        typedef int ARRAS_SOCKET;
        const int ARRAS_INVALID_SOCKET = -1;
        #define SOCKET_CLOSE(x) ::close(x)
        #define SOCKET_CREATE(x,t) x = ::socket(t, SOCK_STREAM, 0); fcntl(x, F_SETFD, FD_CLOEXEC);
        #include <fcntl.h>
    #endif
#endif

#ifdef PLATFORM_WINDOWS

    typedef unsigned int sa_family_t;
    #define UNIX_PATH_MAX 108
    struct sockaddr_un {
        sa_family_t sun_family;
        char sun_path[UNIX_PATH_MAX];
    };
#endif

#ifdef PLATFORM_APPLE
    #define UNIX_PATH_MAX 1024
#endif

#endif
