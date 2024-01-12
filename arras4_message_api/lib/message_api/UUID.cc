// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "UUID.h"

#include <uuid/uuid.h>
#include <iomanip>

namespace arras4 {
    namespace api {

const UUID UUID::null = UUID();

UUID& UUID::operator=(const UUID& other)
{
    mBytes = other.mBytes;
    return *this;
}

void UUID::toString(std::string& str) const
{
    char out[37]; // str rep is 36 chars + trailing \0
    unsigned char* uu = const_cast<unsigned char*>(mBytes.data());
    uuid_unparse_lower(uu,out);
    str.assign(out);
}
 
std::string UUID::toString() const
{
    std::string s;
    toString(s);
    return s;
}
 
void UUID::parse(const std::string& aStrRep)
{
    unsigned char* uu = mBytes.data();
    char *in = const_cast<char*>(aStrRep.c_str());
    if (uuid_parse(in,uu) != 0)
        clear();
}
 
void UUID::regenerate()
{ 
    unsigned char* uu = mBytes.data();
    uuid_generate(uu);
}

/*static*/UUID UUID::generate()
{
    UUID u;
    u.regenerate();
    return u;
}

std::ostream &operator<<(std::ostream &s, const UUID &UUID)
{
	return s << std::hex << std::setfill('0')
             << std::setw(2) << (int)UUID.mBytes[0]
             << std::setw(2) << (int)UUID.mBytes[1]
             << std::setw(2) << (int)UUID.mBytes[2]
             << std::setw(2) << (int)UUID.mBytes[3] << "-"
             << std::setw(2) << (int)UUID.mBytes[4]
             << std::setw(2) << (int)UUID.mBytes[5] << "-"
             << std::setw(2) << (int)UUID.mBytes[6]
             << std::setw(2) << (int)UUID.mBytes[7] << "-"
             << std::setw(2) << (int)UUID.mBytes[8]
             << std::setw(2) << (int)UUID.mBytes[9] << "-"
             << std::setw(2) << (int)UUID.mBytes[10]
             << std::setw(2) << (int)UUID.mBytes[11]
             << std::setw(2) << (int)UUID.mBytes[12]
             << std::setw(2) << (int)UUID.mBytes[13]
             << std::setw(2) << (int)UUID.mBytes[14]
             << std::setw(2) << (int)UUID.mBytes[15]
             << std::setfill(' ') << std::dec;
}

}
}

