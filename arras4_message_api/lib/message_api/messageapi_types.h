// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGEAPITYPESH__
#define __ARRAS4_MESSAGEAPITYPESH__

#include <memory>
#include <list>

namespace arras4 {
    namespace api {

        class UUID;
        class ArrasTime;
        class Address;
        class Message;
        class MessageContent;
        class ObjectContent;
        class DataInStream;
        class DataOutStream;
        class Metadata;

        typedef UUID ClassID;
        typedef std::shared_ptr<Metadata> MetadataPtr;
        typedef std::shared_ptr<const Metadata> MetadataConstPtr;
        typedef std::shared_ptr<MessageContent> MessageContentPtr;
        typedef std::shared_ptr<const MessageContent> MessageContentConstPtr;
        typedef std::list<Address> AddressList;

        enum class Result { Success, Unknown, Ignored, Invalid };
    }
}
#endif
