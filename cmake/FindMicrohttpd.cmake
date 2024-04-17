# Copyright 2023-2024 DreamWorks Animation LLC
# SPDX-License-Identifier: Apache-2.0

message(STATUS "FindMicrohttpd running")

find_path(Microhttpd_INCLUDE_DIRS
  NAMES microhttpd.h
  HINTS $ENV{LIBMICROHTTPD_ROOT}/include /usr/include)

find_library(Microhttpd_LIBRARY
  NAMES microhttpd libmicrohttpd-dll
  HINTS $ENV{LIBMICROHTTPD_ROOT}/lib /lib64)

mark_as_advanced(Microhttpd_INCLUDE_DIR Microhttpd_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Microhttpd
  REQUIRED_VARS Microhttpd_LIBRARY Microhttpd_INCLUDE_DIRS
)

if (Microhttpd_FOUND AND NOT TARGET Microhttpd::Microhttpd)
    add_library(Microhttpd::Microhttpd UNKNOWN IMPORTED)
    set_target_properties(Microhttpd::Microhttpd PROPERTIES
      IMPORTED_LOCATION "${Microhttpd_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Microhttpd_INCLUDE_DIRS}")
endif()

