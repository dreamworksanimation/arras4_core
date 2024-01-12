# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(CmdName execComp)
set(ExportGroup ${PROJECT_NAME}Targets)

add_executable(${CmdName})

target_sources(${CmdName}
    PRIVATE
        ExecComp.cc
        main.cc
)

target_link_libraries(${CmdName}
    PUBLIC
        ${PROJECT_NAME}::network
        ${PROJECT_NAME}::message_impl
        ${PROJECT_NAME}::computation_impl
        ${PROJECT_NAME}::shared_impl
        ${PROJECT_NAME}::routing
        ${PROJECT_NAME}::exceptions
        ${PROJECT_NAME}::arras4_log
        ${PROJECT_NAME}::arras4_athena
        Boost::program_options
        Boost::filesystem
        Libatomic::Libatomic
        pthread
)

# prevent use of crash reporter since it isn't yet supported
target_compile_definitions(${CmdName} PRIVATE DONT_USE_CRASH_REPORTER)


# Set standard compile/link options
ArrasCore_cxx_compile_definitions(${CmdName})
ArrasCore_cxx_compile_features(${CmdName})
ArrasCore_cxx_compile_options(${CmdName})
ArrasCore_link_options(${CmdName})

install(TARGETS ${CmdName}
        EXPORT ${ExportGroup})
