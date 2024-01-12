// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_TIME__
#define __ARRAS4_TIME__

#include <string>

constexpr unsigned MILLION = 1000000;

namespace arras4 {
    namespace api {

// Time represents either
// (a) a time since the "epoch" (posix/unix time), or
// (b) a time interval, in seconds and microseconds
// seconds is represented as a signed integer, which is the
// most compatible and compact, but becomes invalid in 2038...
// Time intervals are always normalized so that, 
// for positive times, 0 <= microseconds < 1E6, and
// for negative times -1E6 < microseconds < 0

class ArrasTime {
public:
    ArrasTime() : seconds(0), microseconds(0) {}
    ArrasTime(int s,int ms) : seconds(s), microseconds(ms) {
        normalize(); 
    }
    static ArrasTime now();
    static const ArrasTime zero;
       
    int seconds;
    int microseconds;

    long toMicroseconds() const { 
        return ((long)seconds)*MILLION + microseconds; 
    }
    void fromMicroseconds(long ms) {
        seconds = (int)(ms / MILLION);
        microseconds = (int)(ms % MILLION); // will be -ve for negative times
    }
    ArrasTime& normalize() { fromMicroseconds(toMicroseconds()); return *this;}
 
 
    ArrasTime& operator+=(const ArrasTime& t) 
        { fromMicroseconds(toMicroseconds() + t.toMicroseconds()); return *this; }
    ArrasTime& operator-=(const ArrasTime& t) 
        { fromMicroseconds(toMicroseconds() - t.toMicroseconds()); return *this; }
    const ArrasTime operator+(const ArrasTime& t) const { return ArrasTime(*this)+=t; }
    const ArrasTime operator-(const ArrasTime& t) const { return ArrasTime(*this)-=t; }

    bool isZero() const { return (seconds==0) && (microseconds==0); }   
    bool operator!=(const ArrasTime& other) const {
        return (seconds != other.seconds) || 
            (microseconds != other.microseconds); 
    }
    bool operator==(const ArrasTime& other) const {
          return (seconds == other.seconds) && 
            (microseconds == other.microseconds); 
    }
    bool operator<(const ArrasTime& other) const 
        { return this->toMicroseconds() < other.toMicroseconds();  }

    // if we are time since epoch, return local date and time of day 
    // dd/mm/yyyy hh:mm:ss
    std::string dateTimeStr() const;

    // if we are time since epoch, return local time of day HH:MM:SS.mmm   
    std::string timeOfDayStr() const; 

    // if we are a time interval, return interval H:MM:SS.mmm
    // negative interval is -(H:MM:SS.mmm)
    std::string intervalStr() const;

    // return string for use as a filename timestamp
    // dd-mm-yy_hh:mm:ss.uuuuuu
    std::string filenameStr() const;
    bool fromFilename(const std::string& s);

};

}
}
#endif
