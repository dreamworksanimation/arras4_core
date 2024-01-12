# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(CmdName runComp)
set(ExportGroup ${PROJECT_NAME}Targets)

add_executable(${CmdName})

target_sources(${CmdName}
    PRIVATE
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
        pthread
)

# Set standard compile/link options
ArrasCore_cxx_compile_definitions(${CmdName})
ArrasCore_cxx_compile_features(${CmdName})
ArrasCore_cxx_compile_options(${CmdName})
ArrasCore_link_options(${CmdName})

install(TARGETS ${CmdName}
        EXPORT ${ExportGroup})
