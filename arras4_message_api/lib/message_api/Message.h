// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGEH__
#define __ARRAS4_MESSAGEH__

#include "messageapi_types.h"
#include "Metadata.h"
#include "MessageContent.h"

namespace arras4 {
    namespace api {

// This is the interface to message objects.
class Message
{
public:
    Message() {}
    Message(const MetadataPtr& metadata,
            const MessageContentConstPtr& content) :
        mMetadata(metadata),
        mContent(content)
        {}

    Object get(const std::string& optionName) const {
        if (mMetadata) return mMetadata->get(optionName);
        else return Object();
    }
   
    std::string describe() const { 
        if (mMetadata) return mMetadata->describe();
        else return "[Empty Message]";
    }

    const MetadataPtr& metadata() const { return mMetadata; }
    MetadataPtr& metadata() { return mMetadata; }
    const MessageContentConstPtr& content() const { return mContent; }
    MessageContentConstPtr& content() { return mContent; }

    template<typename T> std::shared_ptr<const T> contentAs() const
        { return std::dynamic_pointer_cast<const T>(content()); }

    const ClassID& classId() const { 
        if (content()) return content()->classId(); 
        else return ClassID::null;
    }
    unsigned classVersion() const { 
        if (content()) return content()->classVersion(); 
        else return 0; 
    }

private:
    MetadataPtr mMetadata;
    MessageContentConstPtr mContent;

};

}
}
#endif
