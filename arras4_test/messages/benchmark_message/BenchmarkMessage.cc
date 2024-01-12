// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "BenchmarkMessage.h"
#include <message_api/MessageFormatError.h>

#include <openssl/md5.h>
#include <string>

namespace arras4 {
    namespace benchmark {

ARRAS_CONTENT_IMPL(BenchmarkMessage);
 
void BenchmarkMessage::serialize(arras4::api::DataOutStream& to) const
{
    to.write(static_cast<unsigned char>(mType));
    to.write(mFrom);
    to.write(mValue);
}

void BenchmarkMessage::deserialize(arras4::api::DataInStream& from, 
                              unsigned version)
{
    unsigned char type;
    from.read(type);
    mType = static_cast<BenchmarkMessage::MessageType>(type);
    from.read(mFrom);
    from.read(mValue);
}

BenchmarkMessage::BenchmarkMessage() : mType(MessageType::NOOP) 
{
}

BenchmarkMessage::BenchmarkMessage(MessageType type, const std::string& value, const std::string& from) :
    mType(type), mFrom(from), mValue(value)
{
}

BenchmarkMessage::~BenchmarkMessage()
{
}

}
}

