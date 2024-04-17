// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "UUID.h"
#include <iomanip>

#ifdef PLATFORM_WINDOWS
#include "UUID_windows.cc"
#endif

#ifdef PLATFORM_UNIX
#include "UUID_unix.cc"
#endif

namespace arras4 {
    namespace api {

const UUID UUID::null = UUID();

UUID& UUID::operator=(const UUID& other)
{
    mBytes = other.mBytes;
    return *this;
}

std::string UUID::toString() const
{
    std::string s;
    toString(s);
    return s;
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

