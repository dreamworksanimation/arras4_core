// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_ATHENALOGGER_H__
#define __ARRAS_ATHENALOGGER_H__

#include "UdpSyslog.h"

#include <mutex>
#include <string>

#include <arras4_log/Logger.h>

namespace arras4 {
namespace log {

    // style used when emitting log to console
    enum class ConsoleLogStyle {
	Full,       // all the fields sent to Athena
	Short,      // more user friendly short format
	None        // don't emit log to console
    };

    unsigned short constexpr SYSLOG_PORT = 514;

    class AthenaLogger : public Logger
    {
    private:
        AthenaLogger(const std::string& processName,
                     bool useColor=true,
                     const std::string& athenaEnv="prod",
                     const std::string& syslogHost="localhost",
                     unsigned short syslogPort=SYSLOG_PORT);
    public:
        ~AthenaLogger();

        // create an AthenaLogger and set it as the default logger
        // returned by Logger::instance()
        static AthenaLogger& createDefault(const std::string& processName,
                                           bool useColor=true,
                                           const std::string& athenaEnv="prod",
                                           const std::string& syslogHost="localhost",
                                           unsigned short syslogPort=SYSLOG_PORT);

        void setUseColor(bool aUseColor);

        // provide alternate streams for the stdout/stderr backends
        // this is used by the executor which will capture stderr/stdout
        // from computations and routes them through the logger to
        // an alternate stream.
        void setOutStream(std::ostream* aStream);
        void setErrStream(std::ostream* aStream);

        void logStats(const std::string& jsonStr);
        void logStats(Level level, const std::string& jsonStr);

        // Used by computations, where the session ID is global to the process
        void setSessionId(const std::string& sessionId);

	// Support "full" log to console (includes timestamp, pid etc), and
	// "short" log for local mode, when the log is seen by the user
	void setConsoleStyle(ConsoleLogStyle s) { mConsoleStyle = s; }

        void logEvent(const LogEvent& event);

    protected:
        bool mUseColor;

        std::string mLogIdent;
        std::string mStatsIdent;
        std::string mSessionId;

        std::mutex mMutex;
        std::ostream* mOutStream;
        std::ostream* mErrStream;

        UdpSyslog mSyslog;

        unsigned mTestIndex; // used for testing logging reliability

	ConsoleLogStyle mConsoleStyle;

        void log(Level level, const char *message);
        void log_internal(Level level, const char *message);
    };
} 
} 

#endif // __ARRAS_ATHENALOGGER_H__

