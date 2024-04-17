// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_LOG_PLATFORM_H__
#define __ARRAS_LOG_PLATFORM_H__

#ifdef PLATFORM_WINDOWS

    #ifdef log_EXPORTS
        #define LOG_EXPORT __declspec(dllexport)
    #else
        #pragma warning(disable: 4251)
        #define LOG_EXPORT __declspec(dllimport)
    #endif

#else

    #define LOG_EXPORT

#endif

#endif
