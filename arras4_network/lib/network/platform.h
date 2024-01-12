// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PLATFORM_H__
#define __ARRAS4_PLATFORM_H__

// code for windows/mac has been retained
// in this library, but it is currently
// hard-coded to build for Linux, here
#define ARRAS_LINUX 1


#ifdef ARRAS_LINUX
#define ARRAS_SOCK_CLOEXEC SOCK_CLOEXEC
#else
#define ARRAS_SOCK_CLOEXEC 0
#endif

#ifdef ARRAS_WINDOWS
namespace arras4 {
typedef unsigned int ARRAS_SOCKET;
const unsigned int ARRAS_INVALID_SOCKET = (~0);
}
#else
namespace arras4 {
typedef int ARRAS_SOCKET;
const int ARRAS_INVALID_SOCKET = -1;
}
#endif

#ifdef _MSC_VER
typedef unsigned int sa_family_t;
#define UNIX_PATH_MAX 108
struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[UNIX_PATH_MAX];
};

#endif

#endif
