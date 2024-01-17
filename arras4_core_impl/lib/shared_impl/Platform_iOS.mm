// Copyright 2023-2024 DreamWorks Animation LLC
// SPDX-License-Identifier: Apache-2.0

#import <UIKit/UIKit.h>
#include "Platform.h"
#include <string.h>

namespace arras
{
    void
    getPlatformInfo(PlatformInfo& aInfo)
    {
        NSString* name = [[UIDevice currentDevice] name];
        NSString* model = [[UIDevice currentDevice] model];
        NSString* systemName = [[UIDevice currentDevice] systemName];
        NSString* systemVersion = [[UIDevice currentDevice] systemVersion];

        NSLog(@"Name: %@\nModel: %@\nSystem Name: %@\nSystem Version: %@\n", name, model, systemName, systemVersion);

        aInfo.mPlatformType = [name UTF8String];
        aInfo.mPlatformModel = [model UTF8String];
        aInfo.mOSName = [systemName UTF8String];
        aInfo.mOSVersion = [systemVersion UTF8String];
    }

    static char sBuf[4096];
    const char*
    getRootPath(const char* aMorePath)
    {
        NSString* path = [[NSBundle mainBundle] resourcePath];
        strncpy(sBuf, [path UTF8String], 4096);
        if (aMorePath) {
            strncat(sBuf, aMorePath, 4096);
        }

        return sBuf;
    }
}

