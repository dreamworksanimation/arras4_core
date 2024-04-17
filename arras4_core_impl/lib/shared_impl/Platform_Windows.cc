// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Platform.h"
#include <string.h>

#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <windows.h>
#include <winsock2.h>

namespace arras4 {
    namespace impl {

void getPlatformInfo(PlatformInfo& info)
{
    info.mPlatformType = "Windows";
    std::string mPlatformModel = "Unknown";
    std::string mOSName = "Windows";

    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);

    info.mOSVersion = std::to_string(osvi.dwMajorVersion) + "." + std::to_string(osvi.dwMinorVersion); // e.g. "10.0"
    info.mOSRelease = info.mOSVersion + " " + std::to_string(osvi.dwBuildNumber) + " (" + osvi.szCSDVersion + ")";
    info.mOSDistribution = "Windows";
    info.mBriefVersion = info.mOSVersion;
    info.mBriefDistribution = info.mOSDistribution;

    char buf[1024];
    int status = ::gethostname(buf, 1024);
    if (status == 0)
        info.mNodeName = buf;
}

static char sBuf[4096];
const char* getRootPath(const char* aMorePath)
{
    strncpy(sBuf, "\\", 4096);

    if (aMorePath) {
        strncat(sBuf, aMorePath, (4096 - strlen(sBuf)));
    }

    return sBuf;
}

}
}


