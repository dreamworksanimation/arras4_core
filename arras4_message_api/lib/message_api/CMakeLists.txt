# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(LibName message_api)
set(BuildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(InstallIncludeDir message_api)
set(ExportGroup ${PROJECT_NAME}Targets)

add_library(${LibName} SHARED "")
add_library(${PROJECT_NAME}::message_api ALIAS ${LibName})

target_sources(${LibName}
    PRIVATE
        Address.cc 
        ArrasTime.cc 
        ContentRegistry.cc 
        messageapi_names.cc 
        Object.cc 
        UUID.cc
)

set_property(TARGET ${LibName}
    PROPERTY PUBLIC_HEADER
        Address.h 
        ArrasTime.h 
        ContentMacros.h 
        ContentRegistry.h 
        DataInStream.h 
        DataOutStream.h
        Message.h 
        messageapi_names.h 
        messageapi_types.h 
        MessageContent.h 
        MessageFormatError.h 
        Metadata.h 
        Object.h 
        ObjectContent.h 
        UUID.h
)

target_link_libraries(${LibName}
    PUBLIC
        JsonCpp::JsonCpp 
        Libuuid::Libuuid
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
