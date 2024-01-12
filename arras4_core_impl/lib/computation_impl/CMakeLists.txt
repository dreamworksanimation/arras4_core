# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(LibName computation_impl)
set(BuildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(InstallIncludeDir computation_impl)
set(ExportGroup ${PROJECT_NAME}Targets)

add_library(${LibName} SHARED "")
add_library(${PROJECT_NAME}::computation_impl ALIAS ${LibName})

target_sources(${LibName}
    PRIVATE
    CompEnvironmentImpl.cc
    ComputationHandle.cc
    ControlMessageEndpoint.cc
    PerformanceMonitor.cc
)

set_property(TARGET ${LibName}
    PROPERTY PUBLIC_HEADER
        CompEnvironmentImpl.h
        ComputationExitReason.h
        ComputationHandle.h
        ComputationLoadError.h
        ControlMessageEndpoint.h
        PerformanceMonitor.h
)

target_link_libraries(${LibName}
    PUBLIC
        ${PROJECT_NAME}::arras4_log
        ${PROJECT_NAME}::exceptions
        ${PROJECT_NAME}::computation_api
        ${PROJECT_NAME}::message_impl
        ${PROJECT_NAME}::routing
        ${PROJECT_NAME}::chunking
        ${PROJECT_NAME}::core_messages
        ${PROJECT_NAME}::network
        dl
)

target_include_directories(${LibName}
    PUBLIC
        $<BUILD_INTERFACE:${BuildIncludeDir}>
        $<INSTALL_INTERFACE:include>)

# If at Dreamworks add a SConscript stub file so others can use this library.
SConscript_Stub(${LibName})

# Set standard compile/link options
ArrasCore_cxx_compile_definitions(${LibName})
ArrasCore_cxx_compile_features(${LibName})
ArrasCore_cxx_compile_options(${LibName})
ArrasCore_link_options(${LibName})

include(GNUInstallDirs)

install(TARGETS ${LibName}
        EXPORT ${ExportGroup}
        PUBLIC_HEADER DESTINATION include/${InstallIncludeDir})
