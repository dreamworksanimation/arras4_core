// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGEUNCHUNKER_H__
#define __ARRAS4_MESSAGEUNCHUNKER_H__

#include <message_api/messageapi_types.h>
#include <message_api/UUID.h>

#include <vector>
#include <memory>

namespace arras4 {
    namespace impl {

class MessageChunk;
class Envelope;

class MessageUnchunker
{
public:
    // create a new unchunker, from the first received chunk of
    // a given message
    MessageUnchunker(const std::shared_ptr<const MessageChunk>& chunk);

    // call to add subsequent chunks (not the first)
    void addChunk(const std::shared_ptr<const MessageChunk>& chunk);

    // if message is complete, update envout and return true
    bool getUnchunked(Envelope& envOut);

    typedef std::shared_ptr<MessageUnchunker> Ptr;
 
private:

    unsigned mNumChunks;
    api::UUID mInstanceId;
    unsigned mCount;

    std::vector<std::shared_ptr<const MessageChunk>> mChunks;
};

}
}
#endif
