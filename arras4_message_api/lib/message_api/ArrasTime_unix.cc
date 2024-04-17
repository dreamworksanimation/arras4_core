// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <time.h>
#include <sys/time.h>
#include <stdexcept>

namespace {
    constexpr size_t BUF_SIZE = 256;
}

namespace arras4 {
    namespace api {

/*static*/ ArrasTime ArrasTime::now()
{
    timeval t;
    // clock_gettime() is the more up-to-date
    // way to do this, but doesn't seem to
    // be in the runtime I'm linking against...
    gettimeofday(&t, nullptr);
    ArrasTime tnow((int)t.tv_sec, (int)(t.tv_usec));
    return tnow;
}

// if we are time since epoch, return date and time
// dd/mm/yyyy hh:mm:ss.mmm
std::string ArrasTime::dateTimeStr() const
{
    time_t t = (time_t)seconds;
    tm date;
    localtime_r(&t, &date);
    char buf[BUF_SIZE];
    size_t size = strftime(buf, BUF_SIZE,
        "%d/%m/%Y %H:%M:%S",
        &date);
    std::string td(buf, size);
    std::stringstream ss;
    ss << td << "," << std::setfill('0') << std::setw(3) << (microseconds / 1000);
    return ss.str();
}

// if we are time since epoch, return local time of day string HH:MM:SS,mmm
std::string ArrasTime::timeOfDayStr() const
{
    time_t t = (time_t)seconds;
    tm date;
    localtime_r(&t, &date);
    char buf[BUF_SIZE];
    size_t size = strftime(buf, BUF_SIZE,
        "%H:%M:%S",
        &date);
    std::string td(buf, size);
    std::stringstream ss;
    ss << td << "," << std::setfill('0') << std::setw(3) << (microseconds / 1000);
    return ss.str();
}

// date/time without spaces, suitable for a timestamped filename
std::string ArrasTime::filenameStr() const
{
    time_t t = (time_t)seconds;
    tm date;
    localtime_r(&t, &date);
    char buf[BUF_SIZE];
    size_t size = strftime(buf, BUF_SIZE,
        "%Y-%m-%d_%H:%M:%S",
        &date);
    std::string td(buf, size);
    std::stringstream ss;
    ss << td << "," << std::setfill('0') << std::setw(6) << microseconds;
    return ss.str();
}

bool ArrasTime::fromFilename(const std::string& s)
{
    tm date;
    const char* sp = s.c_str();
    char* p = strptime(sp,
        "%Y-%m-%d_%H:%M:%S.",
        &date);
    if (p == nullptr || *p == '\0')
        return false;
    std::string ms(const_cast<const char*>(p));
    try {
        microseconds = std::stoi(ms);
    }
    catch (std::invalid_argument&) {
        return false;
    }
    time_t secs = mktime(&date);
    if (secs == -1)
        return false;
    seconds = (int)secs;
    return true;
}

}
}
