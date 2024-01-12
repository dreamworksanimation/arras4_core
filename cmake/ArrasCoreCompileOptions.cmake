# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0


function(ArrasCore_cxx_compile_options target)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target}
            PRIVATE
                $<$<BOOL:${ABI_SET_VERSION}>:
                    -fabi-version=${ABI_VERSION} # corrects the promotion behavior of C++11 scoped enums and the mangling of template argument packs.
                >
                -fexceptions                    # Enable exception handling.
                -ffast-math                     # Sets the options -fno-math-errno, -funsafe-math-optimizations, -ffinite-math-only,
                                                #     -fno-rounding-math, -fno-signaling-nans, -fcx-limited-range, -fexcess-precision=fast.
                -fno-omit-frame-pointer         # TODO: add a note
                -fno-strict-aliasing            # TODO: add a note
                -fno-var-tracking-assignments   # Turn off variable tracking
                -march=core-avx2                # Specify the name of the target architecture
                -mavx                           # x86 options
                -mfma                           # x86 options
                -msse                           # x86 options
                -pipe                           # Use pipes rather than intermediate files.
                -pthread                        # Define additional macros required for using the POSIX threads library.
                -Wall                           # Enable most warning messages.
                -Wcast-align                    # Warn about pointer casts which increase alignment.
                -Wcast-qual                     # Warn about casts which discard qualifiers.
                -Wdisabled-optimization         # Warn when an optimization pass is disabled.
                -Wextra                         # This enables some extra warning flags that are not enabled by -Wall
                -Woverloaded-virtual            # Warn about overloaded virtual function names.
                -Wno-cast-function-type         # Disable certain warnings that are enabled by -Wall
                -Wno-class-memaccess            # Disable certain warnings that are enabled by -Wall
                -Wno-conversion                 # Disable certain warnings that are enabled by -Wall
                -Wno-maybe-uninitialized        # Disable certain warnings that are enabled by -Wall
                -Wno-narrowing                  # Disable certain warnings that are enabled by -Wall
                -Wno-sign-compare               # Disable certain warnings that are enabled by -Wall
                -Wno-switch                     # Disable certain warnings that are enabled by -Wall
                -Wno-system-headers             # Disable certain warnings that are enabled by -Wall
                -Wno-unknown-pragmas            # Disable certain warnings that are enabled by -Wall
                -Wno-unused-local-typedefs      # Disable certain warnings that are enabled by -Wall
                -Wno-unused-parameter           # Disable certain warnings that are enabled by -Wall
                -Wno-unused-variable            # Disable certain warnings that are enabled by -Wall

                $<$<CONFIG:RELWITHDEBINFO>:
                    -O3                         # the default is -O2 for RELWITHDEBINFO
                >
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        target_compile_options(${target}
            # TODO: Some if not all of these should probably be PUBLIC
            PRIVATE
                -march=core-avx2                # Specify the name of the target architecture
                -fdelayed-template-parsing      # Shader.h has a template method that uses a moonray class which is no available to scene_rdl2 and is only used in moonray+
                -Wno-deprecated-declarations    # disable auto_ptr deprecated warnings from log4cplus-1.
        )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL Intel)
        target_compile_options(${target}
            # TODO: Some if not all of these should probably be PUBLIC
            PRIVATE
                -march=core-avx2                # Specify the name of the target architecture
        )
    endif()
endfunction()
