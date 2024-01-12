// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ConsoleLogger.h"
#include <assert.h>
#include <iostream>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

namespace arras4 {
    namespace log {

ConsoleLogger::ConsoleLogger(const std::string& aProcessName)
    : Logger(aProcessName),
      mTimeLogging(true),
      mDateLogging(true),
      mUseColor(true),
      mOutStream(&std::cout),
      mErrStream(&std::cerr)
{
}

ConsoleLogger::~ConsoleLogger()
{

}

/* static */ ConsoleLogger& ConsoleLogger::instance()
{
    static ConsoleLogger logger("default");
    return logger;
}

void ConsoleLogger::enableTimeLogging(bool enable)
{
    mTimeLogging = enable;
}

void ConsoleLogger::enableDateLogging(bool enable)
{
    mDateLogging = enable;
}

void ConsoleLogger::setErrStream(std::ostream* aErrStream)
{
    mErrStream = aErrStream;
}

void ConsoleLogger::setOutStream(std::ostream* aOutStream)
{
    mOutStream = aOutStream;
}


static const char* sTypes[] = {
    "F ",
    "E ",
    "W ",
    "I ",
    "D ",
    "T "
};

static const char* sColors[] = {
    "\033[31m", // red
    "\033[33m", // yellow/orange
    "\033[35m", // purple
    "\033[36m", // cyan
    "\033[32m", // green
    "\033[34m"  //
};

static const char* sResetColor = "\033[0m";

void ConsoleLogger::log(Level level, const char *message)
{
    timeval now;
    gettimeofday(&now, NULL);

    pid_t pid = getpid();

    // accumulate everything into a string and
    // then print is as a single string to prevent
    // interleaving of log line components between
    // multiple threads
    std::ostringstream s;
    if (mProcessName.length()) {
        if (mUseColor) {
            s << sColors[level];
        }
        if (mTimeLogging || mDateLogging) {
            tm* date = localtime(&now.tv_sec);

            if (mDateLogging) {
                s.width(4);
                s << date->tm_year + 1900 << "-";
                s.width(2);
                s.fill('0');
                s << date->tm_mon + 1 << "-";
                s.width(2);
                s.fill('0');
                s << date->tm_mday << " ";
            }

            if (mTimeLogging) {
                s.width(2);
                s.fill('0');
                s << date->tm_hour << ":";
                s.width(2);
                s.fill('0');
                s << date->tm_min << ":";
                s.width(2);
                s.fill('0');
                s << date->tm_sec << ",";
                s.width(3);
                s.fill('0');
                s << now.tv_usec / 1000 << " ";
            }
        }

        s << sTypes[level] << mProcessName << "[" << pid << "]: ";
    }
    const std::string& threadName = getThreadName();
    if (threadName.length() > 0) {
        s << threadName << ": ";
    }

    s << message;
    if (mUseColor) s << sResetColor;
    s << "\n";

    std::ostream& o = (level > LOG_ERROR ? *mOutStream : *mErrStream);
    o << s.str();
    o.flush();
}

} 
} 

