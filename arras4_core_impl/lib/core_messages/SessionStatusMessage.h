// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_SESSION_STATUS_MESSAGEH__
#define __ARRAS4_SESSION_STATUS_MESSAGEH__

#include <message_api/ContentMacros.h>
#include <string>

namespace arras4 {
    namespace impl {

class SessionStatusMessage : public arras4::api::ObjectContent
{
public:

    ARRAS_CONTENT_CLASS(SessionStatusMessage,"98e3f258-14b7-45f0-87f7-f5382e2ddca1",0);
    
    SessionStatusMessage() {}
    SessionStatusMessage(const std::string& value) :
        mValue(value)
        {}

    ~SessionStatusMessage() {}

    void serialize(arras4::api::DataOutStream& to) const;
    void deserialize(arras4::api::DataInStream& from, unsigned version);

    void dump(std::ostream& aStrm) const;
    void setValue(const std::string& aValue) { mValue = aValue; }
    const std::string& getValue() const { return mValue; }

private:
    std::string mValue;
};

}
}

#endif
