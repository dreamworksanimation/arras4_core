// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PEER_MESSAGE_ENDPOINTH__
#define __ARRAS4_PEER_MESSAGE_ENDPOINTH__

#include <message_api/messageapi_types.h>
#include <network/network_types.h>
 
#include "MessageEndpoint.h"
#include "MessageReader.h"
#include "MessageWriter.h"

#include <network/Peer.h> 
#include <network/BasicFramingSource.h>
#include <network/BasicFramingSink.h>
#include <network/BufferedSource.h>
#include <network/BufferedSink.h>

#include <atomic>

namespace arras4 {
    namespace impl {

class PeerMessageEndpoint : public MessageEndpoint
{
public:
    // traceInfo is output by message tracing:
    // "comp <compid>","node <nodeid>","client none"
    PeerMessageEndpoint(network::Peer& aPeer,
                        bool useRegistry,
                        const std::string& traceInfo);
    ~PeerMessageEndpoint() {}
    Envelope getEnvelope();
    void putEnvelope(const Envelope& env);
    
    void shutdown(); 

    const MessageReader& reader() const { return mReader; }
    const MessageWriter& writer() const { return mWriter; }
    MessageReader& reader() { return mReader; }
    MessageWriter& writer() { return mWriter; }

    // You can bypass reader and writer by going directly
    // to the framed source/sink.This is a "backdoor" that
    // should be used with care : interleaving data
    // from reader/writer with a directly read/written frame
    // will not have pleasant results...
    network::FramedSink* framedSink() { return mFramedSink.get(); }
    network::FramedSource* framedSource() { return mFramedSource.get(); }

private:
    network::Peer& mPeer;

    std::unique_ptr<network::BasicFramingSource> mFramedSource;
    std::unique_ptr<network::BufferedSource> mBufferedSource;
    MessageReader mReader;

    std::unique_ptr<network::BasicFramingSink> mFramedSink;
    std::unique_ptr<network::BufferedSink> mBufferedSink;
    MessageWriter mWriter;
  
    bool mUseRegistry;
    std::atomic<bool> mShutdown;
};

}
}
#endif
    
