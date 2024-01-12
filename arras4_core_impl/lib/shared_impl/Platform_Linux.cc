// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Platform.h"
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

namespace arras4 {
namespace impl {

void
getPlatformInfo(PlatformInfo& aInfo)
{
    static struct utsname info;
    uname(&info);

    aInfo.mNodeName = info.nodename;
    aInfo.mOSName = info.sysname;
    aInfo.mOSVersion = info.version;
    aInfo.mPlatformType = info.machine;
    aInfo.mOSRelease = info.release;

    const char* envar = getenv("ARRAS_OS_VERSION");
    if (envar != nullptr) {
        aInfo.mBriefVersion = envar;
    }
    envar = getenv("ARRAS_OS_DISTRIBUTION");
    if (envar != nullptr) {
        aInfo.mBriefDistribution= envar;
    }

    if (aInfo.mBriefVersion.empty() || aInfo.mBriefDistribution.empty()) {
        // use simple pipe stream to get OS distribution info
        const int buff_size = 1024;
        char buff[buff_size] = {0};
        FILE *fp = popen("lsb_release -a", "r");
        int index = 0;
        if (fp != nullptr) {
            while (fgets(buff, buff_size, fp) != nullptr) {
                aInfo.mOSDistribution += buff;

                // strip off trailing \n
                size_t len = strlen(buff);
                if (buff[len - 1] == '\n') buff[--len] = 0;

                // extract fields
                if (aInfo.mBriefDistribution.empty() &&
                   (strncmp("Distributor ID:\t", buff, 16) == 0)) {
                    aInfo.mBriefDistribution= &buff[16];
                }
                if (aInfo.mBriefVersion.empty() &&
                    (strncmp("Release:\t", buff, 9) == 0)) {
                    aInfo.mBriefVersion= &buff[9];
                }
                index++;
            }
            pclose(fp);
        }
        // make the distribution briefer
        if (aInfo.mBriefDistribution == "RedHatEnterpriseWorkstation") {
            aInfo.mBriefDistribution= "rhat";
        } else if (aInfo.mBriefDistribution == "RedHatEnterpriseServer") {
            aInfo.mBriefDistribution= "rhat";
        } else if (aInfo.mBriefDistribution == "CentOS") {
            aInfo.mBriefDistribution= "centos";
        }
    }
}

static char sBuf[4096];
const char*
getRootPath(const char* aMorePath)
{
    strncpy(sBuf, "", 4096);

    if (aMorePath) {
        strncat(sBuf, aMorePath, (4096-strlen(sBuf)));
    }

    return sBuf;
}

}
}


