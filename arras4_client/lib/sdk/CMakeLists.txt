# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(LibName sdk)

set(BuildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(InstallIncludeDir sdk)
set(ExportGroup ${PROJECT_NAME}Targets)

add_library(${LibName} SHARED "")
add_library(${PROJECT_NAME}::sdk ALIAS ${LibName})

target_sources(${LibName}
    PRIVATE
        FarmHostsForArrasUtils.cc
        FrameUtils.cc
        MultiSession.cc
        sdk.cc
)

set_property(TARGET ${LibName}
    PROPERTY PUBLIC_HEADER
        arras_static_check.h
        FarmHostsForArrasUtils.h
        FrameUtils.h
        MultiSession.h
        sdk.h
        sdk_impl.h
)

target_link_libraries(${LibName}
    PUBLIC
        ${PROJECT_NAME}::http
        ${PROJECT_NAME}::client_api
        ${PROJECT_NAME}::arras4_athena
        ${PROJECT_NAME}::arras4_log
        ${PROJECT_NAME}::network
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
