# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(arras4_athena)
add_subdirectory(arras4_log)

# arras4_crash not supported yet in cmake build, because
# of breakpad dependency
# add_subdirectory(arras4_crash)
