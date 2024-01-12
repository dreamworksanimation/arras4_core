// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_ENVELOPE_H__
#define __ARRAS4_ENVELOPE_H__

#include <message_api/messageapi_types.h>
#include <message_api/Message.h>

#include "MetadataImpl.h"

namespace arras4 {
    namespace impl {

class Envelope
{
public:
    Envelope() : mMetadata(new MetadataImpl) {}

    explicit Envelope(const api::MessageContentConstPtr& content, 
             api::ObjectConstRef& options = api::Object(),
             const api::AddressList& to = api::AddressList())
        : mContent(content),
          mMetadata(new MetadataImpl(content,options)),
          mTo(to)
        {}

    explicit Envelope(const api::MessageContent* content, // note handover
             api::ObjectConstRef& options = api::Object(),
             const api::AddressList& to = api::AddressList())
        : Envelope(api::MessageContentConstPtr(content),options,to) 
        {}

    void clear();

    const api::MessageContentConstPtr& content() const { return mContent; }
    void setContent(const api::MessageContentConstPtr& content)
        { mContent = content; }
    void setContent(const api::MessageContent* content) // note handover
        { mContent = api::MessageContentConstPtr(content); }
    
    template<typename T> std::shared_ptr<const T> contentAs() const
        { return std::dynamic_pointer_cast<const T>(mContent); }

    const api::ClassID& classId() const; 

    unsigned classVersion() const { 
        if (content()) return content()->classVersion(); 
        else return 0; 
    }

    const MetadataImpl::Ptr& metadata() const { return mMetadata; }
    MetadataImpl::Ptr& metadata() { return mMetadata; }
    std::string describe() const { 
        if (mMetadata) return mMetadata->describe();
        else return "[Empty Message]";
    }

    const api::AddressList& to() const { return mTo; }
    api::AddressList& to() { return mTo; }
    void addTo(const api::Address& addr) { mTo.push_back(addr); }
    void clearTo() { mTo.clear(); }

    api::Message makeMessage() { return api::Message(mMetadata,mContent); }

     // read/write message metadata, leaving stream ready
    // to read/write content
    void serialize(api::DataOutStream& to) const;
        // throws MessageFormatError
    void deserialize(api::DataInStream& from, 
                     api::ClassID& classId, 
                     unsigned& version);
        // throws MessageFormatError

    bool isEmpty() const { return !mContent; }

private:

    api::MessageContentConstPtr mContent;
    MetadataImpl::Ptr mMetadata;
    api::AddressList mTo;
};

    }
}
#endif
