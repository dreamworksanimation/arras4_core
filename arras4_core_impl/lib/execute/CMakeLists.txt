# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(LibName execute)
set(BuildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(InstallIncludeDir execute)
set(ExportGroup ${PROJECT_NAME}Targets)

add_library(${LibName} SHARED "")
add_library(${PROJECT_NAME}::execute ALIAS ${LibName})

target_sources(${LibName}
    PRIVATE
        ControlGroup.cc
        Environment.cc
        IoCapture.cc
        process_utils.cc
        Process.cc
        ProcessController.cc
        ProcessManager.cc
        RezContext.cc
        ShellContext.cc
        SpawnArgs.cc
)

set_property(TARGET ${LibName}
    PROPERTY PUBLIC_HEADER
        ControlGroup.h
        Environment.h
        IoCapture.h
        MemoryTracking.h
        process_utils.h
        Process.h
        ProcessController.h
        ProcessManager.h
        RezContext.h
        ShellContext.h
        SpawnArgs.h
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
)

if(EXISTS /usr/lib64/libcgroup.so)
    target_link_libraries(${LibName} PUBLIC cgroup)
    target_compile_definitions(${LibName} PRIVATE ARRAS_ENABLE_CGROUP)
endif()

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
