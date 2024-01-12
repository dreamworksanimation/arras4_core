# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(LibName httpserver)
set(BuildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(InstallIncludeDir httpserver)
set(ExportGroup ${PROJECT_NAME}Targets)

add_library(${LibName} SHARED "")
add_library(${PROJECT_NAME}::httpserver ALIAS ${LibName})

target_sources(${LibName}
    PRIVATE
        HttpServer.cc 
        HttpServerRequest.cc 
        HttpServerResponse.cc
)

set_property(TARGET ${LibName}
    PROPERTY PUBLIC_HEADER
        HttpRequestEvent.h 
        HttpServer.h 
        httpserver_types.h 
        HttpServerException.h 
        HttpServerRequest.h 
        HttpServerResponse.h
)

target_link_libraries(${LibName}
    PUBLIC
        ${PROJECT_NAME}::network
        Microhttpd::Microhttpd
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
