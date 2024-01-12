// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "PingMessage.h"

namespace arras4 {
    namespace impl {

ARRAS_CONTENT_IMPL(PingMessage);

const std::string PingMessage::REQUEST_DEFAULT("");
const std::string PingMessage::REQUEST_ACKNOWLEDGE("acknowledge");
const std::string PingMessage::REQUEST_STATUS("status");

void PingMessage::serialize(api::DataOutStream& to) const
{
    to << mRequestType;
}

void PingMessage::deserialize(api::DataInStream& from, 
                                 unsigned /*version*/)
{
    from >> mRequestType;
}

}
}
 
