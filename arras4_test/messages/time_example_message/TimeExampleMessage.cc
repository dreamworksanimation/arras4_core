// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "TimeExampleMessage.h"

// This is needed for ARRAS_CONTENT_IMPL macro but it's
// included by TimeExampleMessage.h
// #include <message_api/ContentMacros.h>

using namespace arras4;

ARRAS_CONTENT_IMPL(TimeExampleMessage);

void
TimeExampleMessage::serialize(arras4::api::DataOutStream& to) const {
    // could also be to.write(mValue);
    to << mValue;
}

void
TimeExampleMessage::deserialize(arras4::api::DataInStream& from,
                              unsigned version) {
    // could also be from.read(mValue);
    from >> mValue;
}

void
TimeExampleMessage::setValue(const std::string& aValue) {
    mValue = aValue;
}

const std::string&
TimeExampleMessage::getValue() const {
    return mValue;
}

void
TimeExampleMessage::dump(std::ostream& aStrm) const {
    aStrm << getValue();
}

