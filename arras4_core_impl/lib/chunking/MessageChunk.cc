// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MessageChunk.h"

#include <message_api/MessageFormatError.h>

namespace arras4 {
namespace impl {

ARRAS_CONTENT_IMPL(MessageChunk);

MessageChunk::~MessageChunk()
{
    delete[] mPayload;
}

void
MessageChunk::serialize(api::DataOutStream& aTo) const
{
    aTo.write(mProtocolVersion);
    aTo.write(mChunkingMethod);
    aTo.write(mNumberOfChunks);
    aTo.write(mChunkIndex);
    aTo.write(&mOffset,sizeof(mOffset));
    aTo.write(&mUnchunkedSize,sizeof(mUnchunkedSize));
    aTo.write(&internalId, sizeof(internalId));
    aTo.write(internalRoutingName);
    aTo.write(&internalInstanceId,sizeof(internalInstanceId));
    aTo.write(&internalOriginId,sizeof(internalOriginId));
    aTo.write(internalClassVersion);
    aTo.write(mPayloadLength);
    aTo.write(mPayload,mPayloadLength);
}

void
MessageChunk::deserialize(api::DataInStream& aFrom,unsigned)
{
    aFrom.read(mProtocolVersion);
    if (mProtocolVersion != 0) {
        throw api::MessageFormatError("Unknown chunking protocol version in MessageChunk::deserialize");
    }

 
    aFrom.read(mChunkingMethod);
    aFrom.read(mNumberOfChunks);
    aFrom.read(mChunkIndex);
    aFrom.read(&mOffset,sizeof(mOffset));
    aFrom.read(&mUnchunkedSize,sizeof(mUnchunkedSize));
    aFrom.read(&internalId, sizeof(internalId));
    aFrom.read(internalRoutingName);
    aFrom.read(&internalInstanceId,sizeof(internalInstanceId));
    aFrom.read(&internalOriginId,sizeof(internalOriginId));
    aFrom.read(internalClassVersion);
    aFrom.read(mPayloadLength);
    mPayload = new unsigned char[mPayloadLength];
    aFrom.read(mPayload,mPayloadLength);
}

} // namespace engine
} // namespace arras

