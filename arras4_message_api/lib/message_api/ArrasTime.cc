// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ArrasTime.h"


#include <sstream>
#include <iomanip>
#include <string>


#ifdef PLATFORM_WINDOWS
#include "ArrasTime_windows.cc"
#endif

#ifdef PLATFORM_UNIX
#include "ArrasTime_unix.cc"
#endif

namespace arras4 {
    namespace api {

const ArrasTime ArrasTime::zero = ArrasTime();

// if we are a time interval, return interval string H:MM::SS,mmm
// negative interval is -H:MM:SS.mmm
std::string ArrasTime::intervalStr() const
{
    int sec = abs(seconds);
    int min = sec/60;
    int ms = abs(microseconds)/1000;
    std::stringstream ss;
    if (seconds < 0) 
        ss << "-";
    ss << (min/60) << ":"
       << std::setw(2) << (min%60) << ":"
       << std::setw(2) << (sec%60) << ","
       << std::setw(3) << ms;
    return ss.str();
}

}
}
