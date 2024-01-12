# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(LibName network)
set(BuildIncludeDir ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(InstallIncludeDir network)
set(ExportGroup ${PROJECT_NAME}Targets)

add_library(${LibName} SHARED "")
add_library(${PROJECT_NAME}::network ALIAS ${LibName})

target_sources(${LibName}
    PRIVATE
        BasicFramingSink.cc 
        BasicFramingSource.cc 
        BufferedSink.cc 
        BufferedSource.cc
        Encryption.cc
        FileDataSource.cc
        InetSocketPeer.cc
        IPCSocketPeer.cc
        MultiBuffer.cc
        Peer.cc
        SocketPeer.cc
)

set_property(TARGET ${LibName}
    PROPERTY PUBLIC_HEADER
        BasicFramingSink.h
        BasicFramingSource.h
        Buffer.h
        BufferedSink.h
        BufferedSource.h
        DataSink.h
        DataSource.h
        Encryption.h
        FileDataSource.h
        FileError.h 
        Frame.h
        FramingError.h
        InetSocketPeer.h
        InvalidParameterError.h
        IPCSocketPeer.h
        MultiBuffer.h
        network_types.h
        OutOfMemoryError.h
        Peer.h
        PeerException.h
        platform.h
        SocketPeer.h
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
