// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGE_READERH__
#define __ARRAS4_MESSAGE_READERH__

#include "StreamImpl.h"
#include "Envelope.h"

#include <message_api/messageapi_types.h>
#include <network/network_types.h>
 
namespace arras4 {
    namespace impl {

// Reads incoming messages from a data source
class MessageReader
{
public:
    // construct from an existing data source, which
    // must be buffered and framed
    // "traceInfo" is a string output in trace messages to indicate the
    // identity of the sending and receiving processes:
    // "C:<compid>","N:<nodeid>","client"
    MessageReader(network::DetachableBufferSource& source,
                  const std::string& traceInfo);
    ~MessageReader();

    // read the next message from the source. If useRegistry
    // is true, the reader will try to deserialize the content.
    // If it fails, or if 'useRegistry' is false, the content
    // will be delivered as an OpaqueContent instance
    Envelope read(bool useRegistry);

    // if you received a message with OpaqueContent because you
    // specified 'useRegistry=false', call this to convert to
    // deserialized content. Use this to selectively deserialize
    // messages based on their type. If deserialization fails,
    // returns false and message is unaltered. If message was already
    // deserialize (i.e. content is not OpaqueContent) then returns
    // true and the message is unaltered.
    static bool deserializeContent(Envelope& message);
  
    // enable autosaving of read messages to a directory
    void enableAutosave(const std::string& directory);
    void disableAutosave();

private:
    void doAutosave();

    network::DetachableBufferSource& mSource;
    InStreamImpl mInStream;
    std::string mTraceInfo;

    bool mIsAutosaving;
    std::string mAutosaveDirectory;
};

}
}
#endif

