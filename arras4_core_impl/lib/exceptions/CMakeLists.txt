# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(LibName exceptions)
set(BuildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(InstallIncludeDir exceptions)
set(ExportGroup ${PROJECT_NAME}Targets)

add_library(${LibName} INTERFACE "")
add_library(${PROJECT_NAME}::exceptions ALIAS ${LibName})

set_property(TARGET ${LibName}
    PROPERTY PUBLIC_HEADER
        InternalError.h 
        KeyError.h 
        ShutdownException.h
)

target_include_directories(${LibName} 
    INTERFACE 
        $<BUILD_INTERFACE:${BuildIncludeDir}>
        $<INSTALL_INTERFACE:include>)

# If at Dreamworks add a SConscript stub file so others can use this library.
SConscript_Stub(${LibName})

# Set standard compile/link options

include(GNUInstallDirs)

install(TARGETS ${LibName}
        EXPORT ${ExportGroup}
        PUBLIC_HEADER DESTINATION include/${InstallIncludeDir})
