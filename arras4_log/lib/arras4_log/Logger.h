// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_LOGGER_H__
#define __ARRAS_LOGGER_H__

/** \file Logger.h */

#include <string>
#include <sstream>
#include <atomic>

namespace arras4 {
    namespace log {

class LogEvent;
/**
 * \brief Base class for loggers
 * 
 * Logger is a base class for logger objects. Loggers are passed string 
 * *messages*, together with a *log level* for each message. Typically they
 * put the messages where someone can see them (e.g. on the console).
 * Messages might be prefixed by useful stuff like the time, process id and/or
 * thread id.
 *
 * Logger itself maintains a default instance, which is usually created by
 * the program at startup. Logger.h also contains some convenient macros
 * that send messages to the default logger. There are two macro variants : one
 * intended for Arras clients/computations, the other intended for internal
 * Arras use.
 */
class Logger
{
public:

    /// Virtual destructor
    virtual ~Logger();

    /// Get the default logger instance
    static Logger& instance();

    /// The log levels that can be used with any logger
    /// Athena tracing uses an additional set of levels at
    /// ATHENA_TRACE0 (so ATHENA_TRACE1 = ATHENA_TRACE0 + 1...)
    enum Level
    {
        LOG_FATAL = 0,
        LOG_ERROR,  
        LOG_WARN,    
        LOG_INFO,     
        LOG_DEBUG,     
        LOG_TRACE,
        ATHENA_TRACE0 = 1000
    };

    /// log a string message with this logger, if aLevel isn't below this logger's threshold 
    void logMessage(Level aLevel, const std::string& aMessage);

    // TOOD: it is possible for a user to pass an arbitrary/unknown format string that
    // may contain format specifiers (but no arguments); this can cause crashes. The solution is variadic
    // templates to discriminate between zero-arg calls and calls with arguments, and
    // handle the zero-arg calls separately. ICC has a bug that prevents this, and will not be fixed
    // until ICC16: https://software.intel.com/en-us/forums/intel-c-compiler/topic/345355

    /// log a printf-style message with this logger, if aLevel isn't below this logger's threshold 
    void logMessage(Level aLevel, const char* aFormat, ...);

    /// log an event, that contains a message and additional fields
    virtual void logEvent(const LogEvent& event);

    /**
     * \brief Provide an alternate stream for writing logs.
     * \note only really applies to ConsoleLogger and similar : others may ignore this.
     */
    virtual void setErrStream(std::ostream*) {};

    /** 
     * \brief Provide an alternate stream for writing logs.
     * \note only really applies to ConsoleLogger and similar: others may ignore this.
     */
    virtual void setOutStream(std::ostream*) {};

    /** 
     * \brief Set logging verbosity
     *. 
     * Messages STRICTLY BELOW this threshold (more verbose)
     * will be suppressed (default: LOG_ERROR)
     * \param level the threshold to set. 
     * \note LOG_ERROR and LOG_FATAL are always logged.
     */
    void setThreshold(Level level);

    /// Get the current logging threshold.
    Level threshold();

    /// Get the current tracing threshold (~0-5)
    void setTraceThreshold(int threshold);
    int traceThreshold() const;

    /// Set the process name to be used in logs.
    void setProcessName(const std::string& processName) { mProcessName = processName; }

    /** 
     * \brief Set the thread name to be used in logs.
     * \note this is a thread-local setting 
     */
    void setThreadName(const std::string& threadName);

    /**\brief Get the thread name to be used in logs
     * \note this is a thread-local setting 
     */
    std::string getThreadName() const;

    /**
     * \brief Set the default logger instance.
     * \note The default logger is used by the macros and returned by Logger::instance()
     */
    static void setDefault(Logger& logger) { sharedInstance = &logger; }

protected:

    /// Protected constructor, so that only subclasses may be instantiated.
    Logger(const std::string& aProcessName = "default");

    /// This function should be overridden in subclasses to actually log a message
    virtual void log(Level level, const char* message) = 0;

    /// Logs below this threshold (i.e. more verbose) will be omitted)
    Level mThreshold;

    /// Traces below this threshold will be suppressed
    /// (not all loggers support tracing)
    int mTraceThreshold;

    /// The name of the process that owns this logger
    std::string mProcessName;

private:

    /** 
     * \brief The default logger instance (if one has been set)
     * \note the pointer may be null
     */
    static std::atomic<Logger*> sharedInstance;
};

class LogEvent
{
public:
    LogEvent(Logger::Level level) : mLevel(level)
        {}
    
    std::string& module() { return mModule; }
    const std::string& module() const { return mModule; }
    Logger::Level& level() { return mLevel; }
    Logger::Level level() const { return mLevel; }
    std::string& session() { return mSession; }
    const std::string& session() const { return mSession; }
    std::string& id() { return mId; }
    const std::string& id() const { return mId; }
    std::stringstream& messageStream() { return mMessageStream; }
    std::string message() const { return mMessageStream.str(); }

private:
    std::string mModule;
    Logger::Level mLevel;
    std::string mSession;
    std::string mId;
    std::stringstream mMessageStream;
};

}
}

// ARRAS_LOG.. macros should be used by arras client code, such as computations
#define ARRAS_LOG_FATAL(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_FATAL, f, ##__VA_ARGS__);
#define ARRAS_LOG_ERROR(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_ERROR, f, ##__VA_ARGS__);
#define ARRAS_LOG_WARN(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_WARN, f, ##__VA_ARGS__);
#define ARRAS_LOG_INFO(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_INFO, f, ##__VA_ARGS__);
#define ARRAS_LOG_DEBUG(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_DEBUG, f, ##__VA_ARGS__);

// ARRAS_IMPL_LOG... macros should be used by arras internal implementation. Currently there is no
// difference, but this may change in the future.
#define ARRAS_IMPL_LOG_FATAL(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_FATAL, f, ##__VA_ARGS__);
#define ARRAS_IMPL_LOG_ERROR(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_ERROR, f, ##__VA_ARGS__);
#define ARRAS_IMPL_LOG_WARN(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_WARN, f, ##__VA_ARGS__);
#define ARRAS_IMPL_LOG_INFO(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_INFO, f, ##__VA_ARGS__);
#define ARRAS_IMPL_LOG_DEBUG(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_DEBUG, f, ##__VA_ARGS__);

// Define the stream ones
#define ARRAS_LOG_STREAM(TYPE, LOG) \
        { std::stringstream _buf; \
          _buf << LOG; \
          ::arras4::log::Logger::instance().logMessage(TYPE, _buf.str()); }
#define ARRAS_IMPL_LOG_STREAM(TYPE, LOG) \
        { std::stringstream _buf; \
          _buf << LOG; \
          ::arras4::log::Logger::instance().logMessage(TYPE, _buf.str()); }

#define ARRAS_LOG_FATAL_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_FATAL, f);
#define ARRAS_LOG_ERROR_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_ERROR, f);
#define ARRAS_LOG_WARN_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_WARN, f);
#define ARRAS_LOG_INFO_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_INFO, f);
#define ARRAS_LOG_DEBUG_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_DEBUG, f);

#define ARRAS_IMPL_LOG_FATAL_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_FATAL, f);
#define ARRAS_IMPL_LOG_ERROR_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_ERROR, f);
#define ARRAS_IMPL_LOG_WARN_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_WARN, f);
#define ARRAS_IMPL_LOG_INFO_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_INFO, f);
#define ARRAS_IMPL_LOG_DEBUG_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_DEBUG, f);

#if defined(DEBUG)
    #define ARRAS_LOG_TRACE(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_TRACE, f, ##__VA_ARGS__);
    #define ARRAS_LOG_TRACE_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_TRACE, f); 
    #define ARRAS_IMPL_LOG_TRACE(f, ...) ::arras4::log::Logger::instance().logMessage(::arras4::log::Logger::LOG_TRACE, f, ##__VA_ARGS__);
    #define ARRAS_IMPL_LOG_TRACE_STR(f) ARRAS_LOG_STREAM(::arras4::log::Logger::LOG_TRACE, f);
#else // DEBUG
    #define ARRAS_LOG_TRACE(f, ...)
    #define ARRAS_LOG_TRACE_STR(f)
    #define ARRAS_IMPL_LOG_TRACE(f, ...)
    #define ARRAS_IMPL_LOG_TRACE_STR(f)
#endif // DEBUG

#endif

