// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0


#ifndef __ARRAS_HTTPSERVER_PLATFORM_H__
#define __ARRAS_HTTPSERVER_PLATFORM_H__


#if !defined(SOCKET_CLOSE)

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

#endif
