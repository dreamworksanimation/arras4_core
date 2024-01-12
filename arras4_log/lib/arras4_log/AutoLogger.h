// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_AUTOLOGGER_H__
#define __ARRAS_AUTOLOGGER_H__

#include "Logger.h"

#include <iostream>
#include <ext/stdio_filebuf.h>

// Create an instance of AutoLogger to redirect stdout and stderr into
// a logger
namespace arras4 {
    namespace log {
 
        class OutputCapture;

/**
 *\brief Redirects stdout and stderr into a logger.
 *
 * When you construct an AutoLogger object, it will create a redirection pipe that
 * sends stdout and stderr to a specified logger. stdout and stderr will return to
 * normal when the AutoLogger object is destroyed.
 *
 * stdout messages become level LOG_INFO, stderr messages become LOG_ERROR.
 *
 * \note The logger you are redirecting to should not try to write the messages to
 * stdout or stderr, since this could cause a loop. AutoLogger avoids this with
 * ConsoleLogger by calling setOutStream and setErrStream to point the ConsoleLogger 
 * at the original, un-redirected stdout/stderr. This will work with other loggers if
 * they implement setOutStream and setErrStream.
 */
class AutoLogger {
public:
    /// Create an AutoLogger redirecing stdout/stderr to the given logger
    AutoLogger(Logger& logger);
    /// Create an AutoLogger redirecing stdout/stderr to the default logger
    AutoLogger();

    ~AutoLogger();

private:
    void init();

    Logger& mLogger;
    OutputCapture* mStderr;
    __gnu_cxx::stdio_filebuf<char>* mErrBuf;
    std::ostream* mErrStream;
    OutputCapture* mStdout;
    __gnu_cxx::stdio_filebuf<char>* mOutBuf;
    std::ostream* mOutStream;
};

}
}
#endif

