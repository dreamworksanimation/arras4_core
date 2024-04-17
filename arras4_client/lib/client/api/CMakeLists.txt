# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(LibName client_api)
set(BuildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(InstallIncludeDir client/api)
set(ExportGroup ${PROJECT_NAME}Targets)

add_library(${LibName} SHARED "")
add_library(${PROJECT_NAME}::client_api ALIAS ${LibName})

if (IsUnixPlatform)
    set(PlatformSpecificSources
            ProgressSender.cc
            RezResolve.cc)
endif()

target_sources(${LibName}
    PRIVATE
        AcapAPI.cc
        Client.cc
        Component.cc
        SessionDefinition.cc
        ${PlatformSpecificSources}
)

if (IsUnixPlatform)
    set(PlatformSpecificHeaders
            ProgressSender.h
            RezResolve.h)
endif()

set_property(TARGET ${LibName}
    PROPERTY PUBLIC_HEADER
        AcapAPI.h
        Client.h
        ClientException.h
        Component.h
        SessionDefinition.h
        ${PlatformSpecificHeaders}
)

if (IsUnixPlatform)
    set(PlatformSpecificLibs
            ${PROJECT_NAME}::client_local
            ${PROJECT_NAME}::execute)
endif()

target_link_libraries(${LibName}
    PUBLIC
        ${PROJECT_NAME}::network
        ${PROJECT_NAME}::http
        ${PROJECT_NAME}::arras4_log
        ${PROJECT_NAME}::core_messages
        ${PROJECT_NAME}::message_impl
        ${PROJECT_NAME}::shared_impl
        ${PROJECT_NAME}::chunking
        Boost::filesystem
        ${PlatformSpecificLibs}
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

