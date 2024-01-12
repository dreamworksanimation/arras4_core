// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PROCESS_UTILS_H__
#define __ARRAS4_PROCESS_UTILS_H__

#include <sys/types.h>

namespace arras4 {
    namespace impl {

int countProcessGroupMembers(pid_t aPgid);
bool doesProcessGroupHaveMembers(pid_t aPgid);

}
}
#endif
