// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "EngineReadyMessage.h"
#include <message_api/MessageFormatError.h>

namespace arras4 {
    namespace impl {

ARRAS_CONTENT_IMPL(EngineReadyMessage);

void EngineReadyMessage::serialize(api::DataOutStream& to) const
{
    to << mEncoderOutputUri << mSDPData;
}

void EngineReadyMessage::deserialize(api::DataInStream& from, 
                                     unsigned /*version*/)
{
    from >> mEncoderOutputUri >> mSDPData;
}

}
}
