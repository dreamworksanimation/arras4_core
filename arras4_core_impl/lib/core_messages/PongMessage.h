// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PONG_MESSAGEH__
#define __ARRAS4_PONG_MESSAGEH__

#include <message_api/ContentMacros.h>
#include <message_api/UUID.h>
#include <message_api/Object.h>
#include <string>

namespace arras4 {
    namespace impl {

class PongMessage : public arras4::api::ObjectContent
{
public:
    static const std::string RESPONSE_ACKNOWLEDGE;
    static const std::string RESPONSE_STATUS;

    ARRAS_CONTENT_CLASS(PongMessage,"1f8a6b11-0cb1-4bdd-b751-d66f6c71d8e2",0);
    
    PongMessage() : mTimeSecs(0),mTimeMicroSecs(0) {}
    ~PongMessage() {}

    void serialize(arras4::api::DataOutStream& to) const;
    void deserialize(arras4::api::DataInStream& from, unsigned version);

    const api::UUID& sourceId() const { return mSourceId; }
    void setSourceId(const api::UUID& id) { mSourceId = id; }
    const std::string& name() const { return mName; }
    void setName(const std::string& aName) { mName = aName; }
    const std::string& entityType() const { return mEntityType; }
    void setEntityType(const std::string& anEntityType) { mEntityType = anEntityType; }
    const std::string& responseType() const { return mResponseType; }
    void setResponseType(const std::string& aResponseType) { mResponseType = aResponseType; }

    uint64_t timeSecs() const { return mTimeSecs; }
    unsigned int timeMicroSecs() const { return mTimeMicroSecs; }
    void setTime(unsigned long long secs, unsigned int microsecs) { mTimeSecs = secs; mTimeMicroSecs = microsecs; }
    std::string timeString() const;

    bool payload(api::ObjectRef aJson) const;
    const std::string& payload() const { return mPayload; }
    bool setPayload(api::ObjectConstRef aJson);
    void setPayload(const std::string& aString) { mPayload = aString; }
    
private:
    api::UUID mSourceId;
    std::string mName;
    std::string mEntityType;
    std::string mResponseType;
    uint64_t mTimeSecs;
    unsigned int mTimeMicroSecs;
    std::string mPayload;
    
};

}
}

#endif
