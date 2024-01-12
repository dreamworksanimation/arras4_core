// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_STANDALONE_METADATA_H__
#define __ARRAS4_STANDALONE_METADATA_H__

#include <message_api/messageapi_types.h>
#include <message_api/Message.h>
#include <message_api/Metadata.h>

namespace arras4 {
    namespace impl {

class StandaloneMetadata : public api::Metadata
{
public:
    StandaloneMetadata();
    StandaloneMetadata(const api::MessageContentConstPtr& content,
                       api::ObjectConstRef options);

    // virtual overrides to Metadata interface
    api::Object get(const std::string& optionName) const;
    void toObject(api::ObjectRef obj);
    void fromObject(api::ObjectConstRef obj);
    std::string describe() const { return mRoutingName; }

    // implementation functions
    const api::UUID& instanceId() const { return mInstanceId; }
    const api::UUID& sourceId() const { return mSourceId; }
    void setSourceId(const api::UUID& id) { mSourceId = id; }
    const api::ArrasTime& creationTime() const { return mCreationTime; }
    const api::Address& from() const { return mFrom; } 
    api::Address& from() { return mFrom; }
    void setFrom(const api::Address& addr) { mFrom = addr; }
  
    const std::string& routingName() const { return mRoutingName; }
    std::string& routingName() { return mRoutingName; }
    void setRoutingName(const std::string& name) { mRoutingName = name; }
  
    using Ptr = std::shared_ptr<StandaloneMetadata>;
    using ConstPtr = std::shared_ptr<const StandaloneMetadata>;

private:
    api::UUID mInstanceId;
    api::UUID mSourceId;
    api::ArrasTime mCreationTime;
    api::Address mFrom;
    std::string mRoutingName;
};

}
}
#endif
