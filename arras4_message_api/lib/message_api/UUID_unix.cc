// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <uuid/uuid.h>

namespace arras4 {
    namespace api {

void UUID::toString(std::string& str) const
{
     char out[37]; // str rep is 36 chars + trailing \0
     unsigned char* uu = const_cast<unsigned char*>(mBytes.data());
     uuid_unparse_lower(uu, out);
     str.assign(out);
}

void UUID::parse(const std::string& aStrRep)
{
    unsigned char* uu = mBytes.data();
    char* in = const_cast<char*>(aStrRep.c_str());
    if (uuid_parse(in, uu) != 0)
        clear();
}

void UUID::regenerate()
{
    unsigned char* uu = mBytes.data();
    uuid_generate(uu);
}

}
}
