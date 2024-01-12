// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "SessionStatusMessage.h"
#include <message_api/MessageFormatError.h>

namespace arras4 {
    namespace impl {

ARRAS_CONTENT_IMPL(SessionStatusMessage);

void SessionStatusMessage::serialize(api::DataOutStream& to) const
{
    to << mValue;
}

void SessionStatusMessage::deserialize(api::DataInStream& from, 
                                       unsigned /*version*/)
{
    from >> mValue;
}

void SessionStatusMessage::dump(std::ostream& aStrm) const
{
    aStrm << mValue;
}

}
}
 
