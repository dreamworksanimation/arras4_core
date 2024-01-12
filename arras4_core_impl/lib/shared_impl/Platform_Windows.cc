// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <client/detail/Platform.h>
#include <string.h>

namespace arras4
{
    void
    getPlatformInfo(PlatformInfo& aInfo)
    {
        //aInfo.mNodeName = info.nodename;
        //aInfo.mOSName = info.sysname;
        //aInfo.mOSVersion = info.version;
        //aInfo.mPlatformType = info.machine;
    }

    static char sBuf[4096];
    const char*
    getRootPath(const char* aMorePath)
    {
        strncpy(sBuf, "\\", 4096);

        if (aMorePath) {
            strncat(sBuf, aMorePath, (4096-strlen(sBuf)));
        }

        return sBuf;
    }
}


