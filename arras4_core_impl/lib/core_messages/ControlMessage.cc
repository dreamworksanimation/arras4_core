// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ControlMessage.h"
#include <message_api/MessageFormatError.h>

namespace arras4 {
    namespace impl {

ARRAS_CONTENT_IMPL(ControlMessage);

void ControlMessage::serialize(api::DataOutStream& to) const
{
    to << mCommand << mData << mExtra;
}

void ControlMessage::deserialize(api::DataInStream& from, 
                                 unsigned /*version*/)
{
    from >> mCommand >> mData >> mExtra;
}

}
}
 
