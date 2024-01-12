// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MessageUnchunker.h"
#include "MessageChunk.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>
#include <exceptions/InternalError.h>
#include <network/network_types.h>
#include <network/Buffer.h>
#include <network/MultiBuffer.h>
#include <message_impl/Envelope.h>
#include <message_impl/StreamImpl.h>

#include <string.h> // memcpy

using namespace arras4::api;
using namespace arras4::network;

namespace arras4 {
namespace impl {

    // create a new unchunker, from the first received chunk of
    // a given message
    MessageUnchunker::MessageUnchunker(const std::shared_ptr<const MessageChunk>& chunk) :
        mNumChunks(0), mCount(0)
    {
      
        ARRAS_DEBUG("Beginning collection of chunked message " <<
                    chunk->internalInstanceId.toString() << " (" << 
                    chunk->mNumberOfChunks << " chunks)");
        mNumChunks = chunk->mNumberOfChunks;
        if (mNumChunks < 1) {
            ARRAS_ERROR(log::Id("invalidMessageChunk") << "Message chunk contained invalid chunk count of zero");
            throw InternalError("[MessageChunker] Chunk count is less than 1");
        }
        mInstanceId = chunk->internalInstanceId;
        mChunks.resize(mNumChunks);
        mCount = 0;
        addChunk(chunk);
    }

    // call to add subsequent chunks (not the first)
    void MessageUnchunker::addChunk(const std::shared_ptr<const MessageChunk>& chunk)
    {
        ARRAS_DEBUG("Processing Chunk " <<  chunk->mChunkIndex << " of message " << 
                    chunk->internalInstanceId.toString() << " (len " << 
                    chunk->mPayloadLength << " bytes)");
        if (chunk->mNumberOfChunks != mNumChunks ||
            chunk->internalInstanceId != mInstanceId) {
            ARRAS_ERROR(log::Id("invalidMessageChunk") << "Message chunk contained incorrect data");
            throw InternalError("[MessageChunker/addChunk] Chunk data mismatch");
        }
        if (mChunks[chunk->mChunkIndex]) {
            ARRAS_ERROR(log::Id("invalidMessageChunk") << "Message chunk duplicates one already received");
            throw InternalError("[MessageChunker/addChunk] Duplicate chunk received");
        }
        mChunks[chunk->mChunkIndex] = chunk;
        mCount++;
    }

    // if message is complete, update envout and return true
    bool MessageUnchunker::getUnchunked(Envelope& envOut)
    {
        if (mCount < mNumChunks) {
            return false;
        }

        ARRAS_INFO("Chunked message " << mChunks[0]->internalInstanceId.toString() <<
                   " is complete, recreating from " << mNumChunks << " chunks");

        // check that the total length of all the chunks adds up to the
        // specified unchunked size
        uint64_t sumLen = 0;
        for (unsigned i = 0; i < mNumChunks; i++) {
            if (mChunks[i]) 
                sumLen += mChunks[i]->mPayloadLength;
            else {
                ARRAS_ERROR(log::Id("missingMessageChunk") <<
                            "Chunk # " << i << " was missing from chunked message");
                throw InternalError("[MessageUnchunker/getUnchunked] Missing chunk");
            }
        }

    
        // There must be at least one chunk, so chunk 0 is used as a reference
        if (sumLen != mChunks[0]->mUnchunkedSize) {
            ARRAS_ERROR(log::Id("chunkingSizeError") <<
                        "Chunked message size mismatch : expected " << mChunks[0]->mUnchunkedSize << 
                        " bytes, but the total across " << mNumChunks << " chunks was " << sumLen);
                       
            throw InternalError("[MessageUnchunker/getUnchunked] Chunk size mismatch");
        }

        
        // construct the message and deserialize it
        ClassID classId = mChunks[0]->internalId;
        unsigned version = mChunks[0]->internalClassVersion;
        ObjectContent* objContent = ContentRegistry::singleton()->
            create(classId,version);
        if (!objContent) {
            ARRAS_ERROR(log::Id("chunkingClassError") << 
                        "Couldn't recreate chunked message : message class " <<
                        classId.toString() << " could not be instantiated");
                        
            throw InternalError("[MessageUnchunker/getUnchunked] Failed to instantiate message class :" +
                                classId.toString());
        }
    
        // create a multibuffer to deserialize from
        MultiBuffer mb; 
        for (unsigned i = 0; i < mNumChunks; i++) {
            BufferUniquePtr buf(new Buffer(mChunks[i]->mPayload,
                                           mChunks[i]->mPayloadLength,
                                           mChunks[i]->mPayloadLength));
            mb.addBuffer(std::move(buf));
        }

        InStreamImpl stream(mb);        
        objContent->deserialize(stream,version);
        envOut.setContent(objContent);
        return true;
    } 
}
}
