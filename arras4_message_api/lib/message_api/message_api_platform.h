// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_MESSAGE_API_PLATFORM_H__
#define __ARRAS_MESSAGE_API_PLATFORM_H__

#ifdef PLATFORM_WINDOWS

#ifdef message_api_EXPORTS
#define MESSAGE_API_EXPORT __declspec(dllexport)
#else
#pragma warning(disable: 4251)
#define MESSAGE_API_EXPORT __declspec(dllimport)
#endif

#else

#define MESSAGE_API_EXPORT
#endif

#endif
