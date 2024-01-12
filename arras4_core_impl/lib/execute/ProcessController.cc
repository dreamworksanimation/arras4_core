// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ProcessController.h"

namespace arras4 {
    namespace impl {

bool ProcessController::sendStop(const api::UUID& /*id*/, const api::UUID& /*sessionId*/)
{
    return false;
}

}
}
