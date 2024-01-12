// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "AthenaLogger.h"

#include <array>
#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>

namespace arras4 {
namespace log {

const std::string LOG_IDENT_INFIX = "-logs-athena.arras-";
const std::string STATS_IDENT_INFIX = "-athena.arras-";
const std::string STATS_IDENT_SUFFIX = "-stats";
const std::string LOG_CHANNEL = "logs";
const std::string STATS_CHANNEL = "stats";

// maps Logger::Level to syslog priority
// These values are defined by RFC 3164 : BSD syslog Protocol
// We don't want to get them from <syslog.h>, because
// that uses #define to define some of the same names used in
// the Logger::Level enum, making Logger::LOG_INFO etc unusable
const std::array<int,6> LEVEL_TO_PRI = {
    (16<<3) | 2, // LOG_LOCAL0 | LOG_CRIT
    (16<<3) | 3, // LOG_LOCAL0 | LOG_ERR,
    (16<<3) | 4, // LOG_LOCAL0 | LOG_WARNING,
    (16<<3) | 6, // LOG_LOCAL0 | LOG_INFO,
    (16<<3) | 7, // LOG_LOCAL0 | LOG_DEBUG,
    (16<<3) | 7  // LOG_LOCAL0 | LOG_DEBUG
};

const std::array<char, 6> LEVEL_ABBR = {'F', 'E', 'W', 'I', 'D', 'T'};
const std::array<std::string, 6> LEVEL_ANSI_COLORS = {
    "\033[31m", // Fail = red
    "\033[33m", // Error = orange
    "\033[35m", // Warn = purple
    "\033[36m", // Info = cyan
    "\033[32m", // Debug = green
    "\033[34m"  // Trace = blue
};

const std::string ANSI_COLOR_RESET = "\033[0m";

/* static */ AthenaLogger& 
AthenaLogger::createDefault(const std::string& processName,
                            bool useColor,
                            const std::string& athenaEnv,
                            const std::string& syslogHost,
                            unsigned short syslogPort)
{
    static AthenaLogger logger(processName,useColor,athenaEnv,syslogHost,syslogPort);
    setDefault(logger);
    return logger;
}

AthenaLogger::AthenaLogger(const std::string& processName,
                           bool useColor,
                           const std::string& athenaEnv,
                           const std::string& syslogHost,
                           unsigned short syslogPort)
    : Logger(processName)
    , mUseColor(useColor)
    , mLogIdent(athenaEnv + LOG_IDENT_INFIX + processName)
    , mStatsIdent(athenaEnv + STATS_IDENT_INFIX + processName + STATS_IDENT_SUFFIX)
    , mOutStream(&std::cout)
    , mErrStream(&std::cerr)
    , mSyslog(syslogHost,syslogPort)
    , mTestIndex(0)
    , mConsoleStyle(ConsoleLogStyle::Full)
{
}

AthenaLogger::~AthenaLogger()
{
}

void 
AthenaLogger::logEvent(const LogEvent& event)
{
    bool emit;
    if (event.level() < ATHENA_TRACE0)
        emit = event.level() <= mThreshold;
    else
        emit = event.level() <= (mTraceThreshold + ATHENA_TRACE0);

    if (emit) { 
        // standard logging format is [sessionId]: {id} message
        std::string message = event.message();
        if (!event.id().empty()) {
            message = "{"+event.id()+"} "+message;
        }
        // be careful not to duplicate session id prefix...
        if (mSessionId.empty() && !event.session().empty()) {
            message = "["+event.session()+"]: "+message;
        }
        log(event.level(),message.c_str());
    }
}

void 
AthenaLogger::log(Level level, const char* message)
{
    if (mTraceThreshold >= 5) {
        // threshold 5 causes testing of logging reliability by
        // emitting a unique id followed by a counter. If
        // a number is missing in sequence, there is a reliability
        // problem with the logging system...
        std::string s("{trace::logging} sequence ");
        s += mProcessName + " " + std::to_string(mTestIndex);
        mTestIndex++;
        log_internal(LOG_DEBUG,s.c_str());
    }
    log_internal(level,message);
}
            
void
AthenaLogger::log_internal(Level level, const char* message)
{
    std::stringstream s;
        
    unsigned realLevel = level;
    if (realLevel >= ATHENA_TRACE0) {
        realLevel = LOG_DEBUG;
    } else if (realLevel > 5) {
        realLevel = 5;
    }
        
    // add timestamp
    // log message requires microsecond precision
    boost::posix_time::ptime bpnow =  boost::posix_time::microsec_clock::local_time();
    s << boost::posix_time::to_iso_extended_string(bpnow);
    
    // add level, process, thread and session
    pid_t pid = getpid();
    s << " " << LEVEL_ABBR[realLevel]
      << " " << mProcessName
      << "[" << pid << "]:"
      << getThreadName()
      << ": ";     
    if (!mSessionId.empty()) {
        s << "[" + mSessionId + "]: ";
    }

    // and actual message
    s  << message;

    // send to syslog, but skip trace messages
    if (realLevel < LOG_TRACE) {
        tm timeStamp = boost::posix_time::to_tm(bpnow);
        mSyslog.sendMessage(LEVEL_TO_PRI[realLevel],
                            &timeStamp,
                            mLogIdent,
                            s.str());
    }

    // send to console
    if (mConsoleStyle == ConsoleLogStyle::None) {
	return;
    }
    std::ostream& o = (level > Logger::LOG_ERROR ? *mOutStream : *mErrStream);
    // In local mode user sees console logs, so support a shorter format (ARRAS-3632)
    if (mConsoleStyle == ConsoleLogStyle::Short) {
	s.str(message);
    }
    if (mUseColor) {
        o << (LEVEL_ANSI_COLORS[realLevel] + s.str() + ANSI_COLOR_RESET + "\n");
    } else {
        o << (s.str() + "\n");
    }
    o.flush();
}

void
AthenaLogger::logStats(const std::string& jsonStr)
{
    logStats(Logger::LOG_INFO, jsonStr);
}

void
AthenaLogger::logStats(Level level, const std::string& jsonStr)
{ 
    // syslog timestamp requires current time to second precision
    boost::posix_time::ptime bpnow =  boost::posix_time::second_clock::local_time();
    tm timeStamp = boost::posix_time::to_tm(bpnow);
    mSyslog.sendMessage(LEVEL_TO_PRI[level],
                        &timeStamp,
                        mStatsIdent,
                        jsonStr);
}

void
AthenaLogger::setOutStream(std::ostream* stream)
{
    std::lock_guard<std::mutex> guard(mMutex);
    mOutStream = stream;
}

void
AthenaLogger::setErrStream(std::ostream* stream)
{
    std::lock_guard<std::mutex> guard(mMutex);
    mErrStream = stream;
}

void
AthenaLogger::setUseColor(bool aUseColor)
{
    std::lock_guard<std::mutex> guard(mMutex);
    mUseColor = aUseColor;
}

void
AthenaLogger::setSessionId(const std::string& sessionId)
{
    std::lock_guard<std::mutex> guard(mMutex);
    mSessionId = sessionId;
}



} // namespace log
} // namespace arras

