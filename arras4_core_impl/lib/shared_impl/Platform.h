// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4__Platform__
#define __ARRAS4__Platform__

#include <string>

namespace arras4 {
namespace impl {

struct PlatformInfo
{
    std::string mPlatformType; // iPad, Android, etc
    std::string mPlatformModel; // 5 (iPhone), Galaxy Tab III, etc
    std::string mOSVersion; // 6.0 (iOS), 2.6.32 (Linux), etc
    std::string mOSName; // iOS, Linux, etc
    std::string mNodeName; // network name, if any
    std::string mOSRelease; // e.g., "2.6.32-696.10.2.el6.x86_64"
    std::string mOSDistribution; // from calling lsb_release
    std::string mBriefVersion; // e.g. 6.9
    std::string mBriefDistribution; // e.g. centos or  rhat
};
    
void getPlatformInfo(PlatformInfo& aPlatformInfo);
const char* getRootPath(const char* morePath=0);

}
}

#endif /* defined(__ARRAS4__Platform__) */

