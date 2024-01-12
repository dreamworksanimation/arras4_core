// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Peer.h"

namespace arras4 {
    namespace network {

size_t PeerSourceAndSink::read(unsigned char* aBuf, size_t aLen)
{
    mPeer.receive_all_or_throw(aBuf,aLen,"Source read");
    return aLen;
}

size_t PeerSourceAndSink::skip(size_t)
{
    throw PeerException(PeerException::INVALID_OPERATION,"Skip not supported for Peer source");
}
    
size_t PeerSourceAndSink::bytesRead() const
{
    return mPeer.bytesRead();
}
 
size_t PeerSourceAndSink::write(const unsigned char* aBuf, size_t aLen)
{
    mPeer.send_or_throw(aBuf,aLen,"Sink write");
    return aLen;
}

void PeerSourceAndSink::flush()
{
}
 
size_t PeerSourceAndSink::bytesWritten() const
{
    return mPeer.bytesWritten();
}

}
}
