# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0


function(ArrasCore_link_options target)
    target_link_options(${target}
        PRIVATE
            -Wl,--enable-new-dtags              # Use RUNPATH instead of RPATH
    )
endfunction()
