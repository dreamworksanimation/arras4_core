// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ChunkingMessageEndpoint.h"
#include "MessageChunk.h"
#include "MessageUnchunker.h"

#include <message_api/ObjectContent.h>
#include <message_impl/StreamImpl.h>
#include <network/network_types.h>
#include <network/Buffer.h>
#include <network/MultiBuffer.h>
#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>
#include <exceptions/InternalError.h>
#include <message_impl/Envelope.h>

using namespace arras4::api;
using namespace arras4::network;

namespace arras4 {
    namespace impl {

 
Envelope ChunkingMessageEndpoint::getEnvelope()
{

    // keep going until we either receive a non-chunk message
    // or a chunk that completes a message
    while (true) {
        Envelope envelope = mSource.getEnvelope();
        if (envelope.classId() != MessageChunk::ID) {
            return envelope;
        }

        MessageChunk::ConstPtr chunk = envelope.contentAs<MessageChunk>();
        UnchunkerMap::iterator it = mUnchunkers.find(chunk->internalInstanceId);
        MessageUnchunker* unchunker;
        if (it == mUnchunkers.end()) {
            unchunker = new MessageUnchunker(chunk);
            it = mUnchunkers.insert(UnchunkerMap::value_type(chunk->internalInstanceId,
                                                             MessageUnchunker::Ptr(unchunker)))
                .first;
        } else {
            unchunker = it->second.get();
            unchunker->addChunk(chunk);
        }
        if (unchunker->getUnchunked(envelope))
        {
            mUnchunkers.erase(it);
            return envelope;
        } 
    }
}

void ChunkingMessageEndpoint::putEnvelope(const Envelope& envelope)
{
    if (!mConfig.enabled) {
        mSource.putEnvelope(envelope);
        return;
    }
        
    std::shared_ptr<const ObjectContent> content = envelope.contentAs<ObjectContent>();  
    if (content == nullptr) {
        // can't chunk messages that aren't ObjectContent
        mSource.putEnvelope(envelope);
        return;
    }

    // Check if the serialized form of the content is long enough to need chunking
    // serializedLength is optional : many subclasses will return 0,
    // making them unchunkable
    size_t unchunkedSize = content->serializedLength();
    if (unchunkedSize < mConfig.minChunkingSize) {
        mSource.putEnvelope(envelope);
        return;
    }

    // serialize the content into a MultiBuffer that will
    // split it into chunks
    MultiBuffer mb(mConfig.chunkSize,mConfig.chunkSize);
    OutStreamImpl stream(mb);
    content->serialize(stream);
    stream.flush();

    size_t numChunks64 = mb.bufferCount();
    if (numChunks64 > UINT16_MAX)
        throw InternalError("[ChunkingMessageEndpoint/putEnvelope] Message is too large for chunking");
    uint16_t numChunks = static_cast<uint16_t>(numChunks64);

    ARRAS_INFO("Message " << envelope.metadata()->instanceId().toString() <<
               " length " << unchunkedSize << " will be broken into " <<
               numChunks << " chunks of size <= " << mConfig.chunkSize);

    for (uint16_t index = 0; index < numChunks; index++)
    {
        MessageChunk::Ptr chunk = std::make_shared<MessageChunk>();
        Envelope chunkEnv(chunk);
        chunkEnv.metadata() = envelope.metadata();
        chunkEnv.to() = envelope.to();

        chunk->mChunkingMethod = 0;
        chunk->mNumberOfChunks = numChunks;
        chunk->mChunkIndex = index;
        chunk->mOffset = index*mConfig.chunkSize;
        chunk->mUnchunkedSize = unchunkedSize;
        chunk->internalId = envelope.classId();
        chunk->internalRoutingName = envelope.metadata()->routingName();
        chunk->internalInstanceId = envelope.metadata()->instanceId();
        chunk->internalOriginId = envelope.metadata()->sourceId();
        chunk->internalClassVersion = envelope.classVersion();

        // grab the buffer from the multibuffer and move it to the message
        BufferUniquePtr buf(mb.takeBuffer(index));
        chunk->mPayloadLength = static_cast<unsigned>(buf->bytesWritten());
        chunk->mPayload = buf->initial();
        buf->releaseData(); // prevents buffer cleaning it up

        // send message chunk to source
        mSource.putEnvelope(chunkEnv);
    }
}

}
}

  
