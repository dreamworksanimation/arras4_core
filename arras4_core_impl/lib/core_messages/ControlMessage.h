// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CONTROL_MESSAGEH__
#define __ARRAS4_CONTROL_MESSAGEH__

#include <message_api/ContentMacros.h>
#include <string>

namespace arras4 {
    namespace impl {

class ControlMessage : public arras4::api::ObjectContent
{
public:

    ARRAS_CONTENT_CLASS(ControlMessage,"0f5db360-a67d-4485-b6a4-e1652a399925",0);
    
    ControlMessage() {}
    ControlMessage(const std::string& cmd) :
        mCommand(cmd)
        {}

    ControlMessage(const std::string& cmd,
                   const std::string& data,
                   const std::string& extra) :
        mCommand(cmd), 
        mData(data),
        mExtra(extra)
        {}

    ~ControlMessage() {}

    void serialize(api::DataOutStream& to) const;
    void deserialize(api::DataInStream& from, unsigned version);

    const std::string& command() const { return mCommand; }
    const std::string& data() const { return mData; }
    const std::string& extra() const { return mExtra; }
    
private:
    std::string mCommand;
    std::string mData;
    std::string mExtra;
};

}
}

#endif
