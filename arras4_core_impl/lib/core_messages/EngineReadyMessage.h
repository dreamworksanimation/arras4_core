// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_ENGINE_READY_MESSAGEH__
#define __ARRAS4_ENGINE_READY_MESSAGEH__

#include <message_api/ContentMacros.h>
#include <string>

namespace arras4 {
    namespace impl {

class EngineReadyMessage : public arras4::api::ObjectContent
{
public:

    ARRAS_CONTENT_CLASS(EngineReadyMessage,"5ed7ec10-3386-452f-b138-aa9f52d581af",0);
    EngineReadyMessage() {}
    EngineReadyMessage(const std::string& aEncoderOutputUri, const std::string& aSDPData) :
        mEncoderOutputUri(aEncoderOutputUri), 
        mSDPData(aSDPData)
        {}

    ~EngineReadyMessage() {}

    void serialize(arras4::api::DataOutStream& to) const;
    void deserialize(arras4::api::DataInStream& from, unsigned version);

    const std::string& encoderOutputUri() const { return mEncoderOutputUri; }
    const std::string& SDPData() const { return mSDPData; }
    
private:
    std::string mEncoderOutputUri;
    std::string mSDPData;
};

}
}

#endif
