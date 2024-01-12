// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_CONSOLELOGGER_H__
#define __ARRAS_CONSOLELOGGER_H__

/** \file Logger.h */

#include <iostream>
#include "Logger.h"

namespace arras4 {
namespace log {

/**
 * \brief Writes logs to stdout and stderr streams
 * 
 * This subclass of Logger writes messages to stdout and stderr, which often causes them
 * to appear in a terminal/console window. LOG_ERROR and LOG_FATAL go to stdout, the other levels to stderr.
 *
 * ConsoleLogger has the ability to prefix the messages
 * with date and/or time, and can also use color control characters to improve readability.
 * 
 * You can set ConsoleLogger to write to any std::ostreams in place of stdout/stderr
 */
    class ConsoleLogger : public Logger
    {
    public:
        /// Constructor, specifying the process name
        ConsoleLogger(const std::string& aProcessName);
        /// Virtual destructor
        ~ConsoleLogger();

        /// Sets whether logs are prefixed by the time (default: true)
        void enableTimeLogging(bool enable);

         /// Sets whether logs are prefixed by the date (default: true)
        void enableDateLogging(bool enable);

        void setErrStream(std::ostream* aErrStream);
        void setOutStream(std::ostream* aErrStream);

        ///\brief Sets whether logs use color (default: true)
        ///\note You can set this to false for easier text parsing of logs
        void setUseColor(bool aUseColor) { mUseColor = aUseColor; }

        /// Returns a singleton default instance of ConsoleLogger
        static ConsoleLogger& instance();

    protected:
   
        void log(Level level, const char *message);

        bool mTimeLogging;
        bool mDateLogging;
        bool mUseColor;
        std::ostream* mOutStream;
        std::ostream* mErrStream;
    };
} 
} 

#endif 

