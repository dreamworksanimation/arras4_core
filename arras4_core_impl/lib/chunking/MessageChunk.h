// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGECHUNK_H__
#define __ARRAS4_MESSAGECHUNK_H__

#include <message_api/ContentMacros.h>
#include <message_api/UUID.h>
#include <string>

namespace arras4 {
    namespace impl {

class MessageChunk : public arras4::api::ObjectContent
{
public:
    ARRAS_CONTENT_CLASS(MessageChunk,"164a8601-dbf7-42e5-b469-3ad1c58dbe83",0);
    ~MessageChunk();

    void serialize(arras4::api::DataOutStream& to) const;
    void deserialize(arras4::api::DataInStream& from, unsigned version);
    
    uint16_t mProtocolVersion = 0;
    uint16_t mChunkingMethod = 0;
    uint16_t mNumberOfChunks;
    uint16_t mChunkIndex;
    uint64_t mOffset;
    uint64_t mUnchunkedSize;

    // this is a copy of the first part of Message::Header, up to the addresses
    api::UUID internalId;
    std::string internalRoutingName;
    api::UUID internalInstanceId;
    api::UUID internalOriginId;
    unsigned internalClassVersion;

    // payload of this chunk
    unsigned int mPayloadLength=0;
    unsigned char* mPayload=0; // array

};

} 
}

#endif 
