// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CHUNKING_MESSAGE_ENDPOINTH__
#define __ARRAS4_CHUNKING_MESSAGE_ENDPOINTH__

#include "ChunkingConfig.h"
#include <message_api/UUID.h>
#include <message_impl/MessageEndpoint.h>
#include <memory>

// This is a message endpoint filter that handles message chunking
namespace arras4 {
    namespace impl {

class MessageUnchunker;
class Envelope;

class ChunkingMessageEndpoint : public MessageEndpoint
{
public:
    ChunkingMessageEndpoint(MessageEndpoint& source,
                            const ChunkingConfig& config = ChunkingConfig()) :
        mConfig(config),
        mSource(source)
        {}

    Envelope getEnvelope();
    void putEnvelope(const Envelope& env);
    void shutdown() { mSource.shutdown(); }

private:
    ChunkingConfig mConfig;
    MessageEndpoint& mSource;
    typedef std::map<api::UUID,std::shared_ptr<MessageUnchunker>> UnchunkerMap;
    UnchunkerMap mUnchunkers;
};

}
}
#endif
