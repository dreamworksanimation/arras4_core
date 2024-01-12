// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_SYSLOGLOGGER_H__
#define __ARRAS_SYSLOGLOGGER_H__

#include "Logger.h"
#include <string>

namespace arras4 {
namespace log {
/**
 * \brief Writes logs to the system logger (syslog)
 */
class SyslogLogger : public Logger
{
public:
    SyslogLogger(const std::string& processName);
    ~SyslogLogger();

protected:
    void log(Level level, const char *message);
};

} 
}

#endif 

