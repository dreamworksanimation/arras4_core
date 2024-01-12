// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGE_WRITERH__
#define __ARRAS4_MESSAGE_WRITERH__

#include "StreamImpl.h"

#include <message_api/messageapi_types.h>
#include <network/network_types.h>
 
        
namespace arras4 {
    namespace impl {

        class Envelope;

// Writes messages out to a data sink
class MessageWriter
{
public:
    // construct from an existing data sink, which
    // must be buffered and framed
    // "traceInfo" is a string output in trace messages to indicate the
    // identity of the sending and receiving processes:
    // "C:<compid>","N:<nodeid>","client"
    MessageWriter(network::AttachableBufferSink& sink,
                  const std::string& traceInfo);
    ~MessageWriter();

    // enable autosaving of written messages to a directory
    void enableAutosave(const std::string& directory);
    void disableAutosave();

    void write(const Envelope& env);

private:
    void doAutosave();

    network::AttachableBufferSink& mSink;
    OutStreamImpl mOutStream;
    std::string mTraceInfo;

    bool mIsAutosaving;
    std::string mAutosaveDirectory;
   
};

}
}
#endif
