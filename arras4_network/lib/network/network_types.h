// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_NETWORK_TYPESH__
#define __ARRAS4_NETWORK_TYPESH__

#include <memory>

namespace arras4 {
    namespace network {

        class Buffer;
        class MultiBuffer;
        class DataSource;
        class DataSink;
        class Peer;
        class SocketPeer;
        class IPCSocketPeer;
        class InetSocketPeer;
        class FramedSource;
        class FramedSink;
        class AttachableBufferSink;
        class DetachableBufferSource;        
        class BasicFramingSource;
        class BasicFramingSink;
        class BufferedSource;
        class BufferedSink;

        typedef std::shared_ptr<Buffer> BufferPtr;
        typedef std::shared_ptr<const Buffer> BufferConstPtr; 
        typedef std::unique_ptr<Buffer> BufferUniquePtr;

        typedef std::shared_ptr<MultiBuffer> MultiBufferPtr;
        typedef std::shared_ptr<const MultiBuffer> MultiBufferConstPtr; 
        typedef std::unique_ptr<MultiBuffer> MultiBufferUniquePtr;
}
}
#endif

