// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "PeerMessageEndpoint.h"
#include <exceptions/ShutdownException.h>


using namespace arras4::network;

namespace arras4 {
    namespace impl {

PeerMessageEndpoint::PeerMessageEndpoint(network::Peer& aPeer,
                                         bool useRegistry,
                                         const std::string& traceInfo)
    : mPeer(aPeer),
      mFramedSource(new BasicFramingSource(aPeer.source())),
      mBufferedSource(new BufferedSource(*mFramedSource)),
      mReader(*mBufferedSource,traceInfo),
      mFramedSink(new BasicFramingSink(aPeer.sink())),
      mBufferedSink(new BufferedSink(*mFramedSink)),
      mWriter(*mBufferedSink,traceInfo),
      mUseRegistry(useRegistry),
      mShutdown(false)
{
}

Envelope PeerMessageEndpoint::getEnvelope() 
{
    if (mShutdown)
        throw ShutdownException("PeerMessageEndpoint was shut down");
    try {
        return mReader.read(mUseRegistry);
    } catch (network::PeerDisconnectException&) {
        // if the read is in progress during a 
        // shutdown, the Peer will generally
        // throw a PeerDisconnectedException
        if (mShutdown)
            throw ShutdownException("PeerMessageEndpoint was shut down");
        throw;
    }
}
    
void  PeerMessageEndpoint::putEnvelope(const Envelope& env) 
{
    if (mShutdown)
        throw ShutdownException("PeerMessageEndpoint was shut down");
    try {
        return mWriter.write(env);
    } catch (network::PeerDisconnectException&) {
        if (mShutdown)
            throw ShutdownException("PeerMessageEndpoint was shut down");
        else
            throw;
    }
}


void  PeerMessageEndpoint::shutdown() {
    // terminate any blocked calls to getEnvelope() or 
    // putEnvelope() with a ShutdownException, and
    // disconnect the Peer socket

    mShutdown = true;
    mPeer.threadSafeShutdown();
    // threadSafeShutdown will force all blocked reads and writes
    // on the peer to throw PeerDisconnectedException, which
    // we modify to ShutdownException above
}

}
}
