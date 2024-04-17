// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include "windows.h"

#include <time.h>

namespace {
	constexpr size_t BUF_SIZE = 256;
}

namespace arras4 {
	namespace api {

/*static*/ ArrasTime ArrasTime::now()
{
	// This is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	long sec = (long)((time - EPOCH) / 10000000L);
	long usec = (long)(system_time.wMilliseconds * 1000);

	ArrasTime tnow((int)sec, (int)usec);
	return tnow;
}

// if we are time since epoch, return date and time
// dd/mm/yyyy hh:mm:ss.mmm
std::string ArrasTime::dateTimeStr() const
{
	time_t t = (time_t)seconds;
	tm date;
	localtime_s(&date, &t);
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
	localtime_s(&date, &t);
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
	localtime_s(&date, &t);
	char buf[BUF_SIZE];
	size_t size = strftime(buf, BUF_SIZE,
		"%Y-%m-%d_%H:%M:%S",
		&date);
	std::string td(buf, size);
	std::stringstream ss;
	ss << td << "," << std::setfill('0') << std::setw(6) << microseconds;
	return ss.str();
}

}
}
