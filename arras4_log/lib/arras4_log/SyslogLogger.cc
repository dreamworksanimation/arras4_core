// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "SyslogLogger.h"

#include <cassert>
#include <syslog.h> 
// warning : including syslog.h makes Logger::LOG_FATAL etc unusable, because
// it overrides them with #defines

namespace arras4 {
namespace log {

SyslogLogger::SyslogLogger(const std::string& aProcessName)
    : Logger(aProcessName)
{
}


SyslogLogger::~SyslogLogger()
{
    closelog();
}

static int sPriorities[] = {
    LOG_CRIT,       // Logger::LOG_FATAL
    LOG_ERR,        // Logger::LOG_ERROR
    LOG_WARNING,    // Logger::LOG_WARN
    LOG_INFO,       // Logger::LOG_INFO
    LOG_DEBUG,      // Logger::LOG_DEBUG
    LOG_DEBUG,      // Logger::LOG_TRACE
};

void SyslogLogger::log(Level level, const char *message)
{
    syslog(sPriorities[level], message);
}

}
}

