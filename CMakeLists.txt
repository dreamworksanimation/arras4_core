# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required (VERSION 3.23.1)

include(packageVersion)  # Sets versionString, projectString and PACKAGE_NAME
set(projectString ArrasCore) # Override projectString
project(${projectString}
    VERSION ${versionString}
    LANGUAGES CXX)

string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
list(APPEND CMAKE_MESSAGE_CONTEXT ${PROJECT_NAME})
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ================================================
# Options
# ================================================
option(${PROJECT_NAME_UPPER}_BUILD_TESTING
    "Whether or not to build the unittests" YES)

option(ABI_SET_VERSION "Enable the abi-version option" OFF)
if(ABI_SET_VERSION)
    set(ABI_VERSION "6" CACHE STRING "If ABI_SET_VERSION is on, which version to set")
endif()

# ================================================
# Find dependencies
# ================================================
set(BUILD_SHARED_LIBS ON)
find_package(Boost REQUIRED
    COMPONENTS
    chrono
    date_time
    filesystem
    program_options
    system)

find_package(Libuuid REQUIRED)
find_package(Libcurl REQUIRED)
find_package(Microhttpd REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Libatomic REQUIRED)
if(${PROJECT_NAME_UPPER}_BUILD_TESTING)
    find_package(CppUnit REQUIRED)
endif()

# ================================================
# Set the RPATH for binaries in the install tree
# ================================================
set(CMAKE_INSTALL_RPATH $ORIGIN $ORIGIN/../lib64)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# if OPT_LEVEL is set use its value to override the CMAKE_BUILD_TYPE because the
# rez cmake plugin does not have an easy way to set the build_target.
set(opt_level $ENV{OPT_LEVEL})
if(opt_level STREQUAL opt-debug)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
elseif(opt_level STREQUAL debug)
    set(CMAKE_BUILD_TYPE Debug)
elseif(opt_level STREQUAL opt)
    set(CMAKE_BUILD_TYPE Release)
endif()

# default to Release if no build type specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Create include/${PACKAGE_NAME} link in the build directory for generated headers.
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/include)
file(CREATE_LINK ../lib ${PROJECT_BINARY_DIR}/include/${PACKAGE_NAME} SYMBOLIC)

include(${PROJECT_NAME}CompileDefinitions)
include(${PROJECT_NAME}CompileFeatures)
include(${PROJECT_NAME}CompileOptions)
include(${PROJECT_NAME}LinkOptions)
include(SConscriptStub)

# ================================================
# Add project files
# ================================================

add_subdirectory(arras4_message_api)
add_subdirectory(arras4_computation_api)
add_subdirectory(arras4_log)
add_subdirectory(arras4_network)
add_subdirectory(arras4_core_impl)
add_subdirectory(arras4_client)
add_subdirectory(arras4_test)
if(GLD STREQUAL "$ENV{STUDIO}")
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/scons/SDKScript
        DESTINATION
        .)
endif()

# ================================================
# Install
# ================================================
include(GNUInstallDirs)
install(EXPORT ${PROJECT_NAME}Targets
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
        NAMESPACE ${PROJECT_NAME}::)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
)

write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION}
)

