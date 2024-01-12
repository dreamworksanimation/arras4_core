// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PING_MESSAGEH__
#define __ARRAS4_PING_MESSAGEH__

#include <message_api/ContentMacros.h>
#include <string>

namespace arras4 {
    namespace impl {

class PingMessage : public arras4::api::ObjectContent
{
public:
    static const std::string REQUEST_DEFAULT;
    static const std::string REQUEST_ACKNOWLEDGE;
    static const std::string REQUEST_STATUS;

    ARRAS_CONTENT_CLASS(PingMessage, "a400811c-524a-4c8a-b316-55af530fc3ca",0);
    
    PingMessage() {}
    ~PingMessage() {}

    void serialize(arras4::api::DataOutStream& to) const;
    void deserialize(arras4::api::DataInStream& from, unsigned version);

    const std::string& requestType() const { return mRequestType; }
    void setRequestType(const std::string& rt) { mRequestType = rt; }
    
private:
    std::string mRequestType;
};

}
}

#endif
