// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Envelope.h"

#include <message_api/DataOutStream.h>
#include <message_api/DataInStream.h>

namespace arras4 { 
    namespace impl {

const api::ClassID& Envelope::classId() const
{
    if (content())
        return content()->classId();
    else
        return api::ClassID::null;
}

void Envelope::serialize(api::DataOutStream& to) const 
{ 
    if (mMetadata) 
        mMetadata->serialize(to,mTo); 
    to << classId() << classVersion();
}
       
void Envelope::deserialize(api::DataInStream& from, 
                           api::ClassID& classId,
                           unsigned& version)
{ 
    if (mMetadata) 
        mMetadata->deserialize(from,mTo); 
    from >> classId >> version;
}

void Envelope::clear()
{
    mTo = api::AddressList();
    mMetadata.reset();
    mContent.reset();
}

}
}
