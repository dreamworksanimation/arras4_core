# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(computation_api)
if(IsUnixPlatform)
    add_subdirectory(standalone)
endif()
