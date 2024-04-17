// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_CLIENT_API_PLATFORM_H__
#define __ARRAS_CLIENT_API_PLATFORM_H__

#ifdef PLATFORM_WINDOWS

    #ifdef client_api_EXPORTS
        #define CLIENT_API_EXPORT __declspec(dllexport)
    #else
        #pragma warning(disable: 4251)
        #define CLIENT_API_EXPORT __declspec(dllimport)
    #endif

// FEATURE_LOCAL_SESSIONS not implementedfor Windows
// FEATURE_REZ_RESOLVE not implemented for Windows
// FEATURE_PROGRESS_SENDER not implemented for Windows

#else

#define CLIENT_API_EXPORT

#define FEATURE_LOCAL_SESSIONS
#define FEATURE_REZ_RESOLVE
#define FEATURE_PROGRESS_SENDER

#endif

#endif
