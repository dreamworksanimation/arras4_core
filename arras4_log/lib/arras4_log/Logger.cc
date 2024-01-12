// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Logger.h"
#include "ConsoleLogger.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib> // std::getenv
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace arras4 {
    namespace log {

// this is defined here rather than as part of Logger so
// that the use of thread locals is completely hidden from
// code which uses Logger
// Using unique_ptr so during shutting down, threads gets destroyed,
// the smart pointer can find out if the variable is de-allocated.
class ThreadName {
  public:
    thread_local static std::unique_ptr<std::string> mThreadName;
};

thread_local std::unique_ptr<std::string> ThreadName::mThreadName;

std::atomic<Logger*> Logger::sharedInstance(nullptr);

Logger::Logger(const std::string& aProcessName)
    : mThreshold(LOG_WARN),
      mTraceThreshold(0),
      mProcessName(aProcessName)
{
    const char* envLogLevel = std::getenv("ARRAS_LOG_LEVEL");
    if (envLogLevel != nullptr) {
        try {
            int logLevel = std::stoi(envLogLevel);
            if (logLevel > LOG_FATAL && logLevel <= LOG_TRACE) {
                setThreshold(static_cast<log::Logger::Level>(logLevel));
            } else {

            }
        } catch (const std::exception& e) {
            // Can't use the logger until we are fully instantiated
            std::cerr << "Error converting ARRAS_LOG_LEVEL environment variable override: "
                      << envLogLevel
                      << " to an integer log level: " << e.what()
                      << std::endl;
        }

    }
}

Logger::~Logger()
{
}

Logger&
Logger::instance()
{
    // If our process/so hasn't set up a specific logger, then send back a default console logger
    if (sharedInstance == nullptr) {
        return ConsoleLogger::instance();
    }
    // sharedInstance is atomic and never set back to null, so this is safe
    return *sharedInstance;
}

void
Logger::setThreshold(Level level)
{
    mThreshold = level;
}

Logger::Level
Logger::threshold()
{
    return mThreshold;
}

void
Logger::setTraceThreshold(int threshold)
{
    mTraceThreshold = threshold;
}

int
Logger::traceThreshold() const
{
    return mTraceThreshold;
}

void
Logger::setThreadName(const std::string& threadName)
{
    // Reminder: mThreadName is thread local
	ThreadName::mThreadName.reset(new std::string(threadName));
}

std::string Logger::getThreadName() const
{
    // Use pid if thread name was de-allocated
    if (bool(ThreadName::mThreadName) == false || ThreadName::mThreadName->length() == 0) {
        // if no thread name has been given (or if thread_local isn't
        // supported) then use the thread id
        std::ostringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    } else {
        return *(ThreadName::mThreadName.get());
    }
}

void
Logger::logMessage(Level level, const std::string& aMessage)
{
    if (level <= mThreshold) {
        log(level, aMessage.c_str());
    }
}

void
Logger::logMessage(Level level, const char *aFormat, ...)
{
    if (level > mThreshold)
        return;

    char buf[16 * 1024];
    va_list va;
    va_start(va, aFormat);
    vsnprintf(buf, 16 * 1024, aFormat, va);
    va_end(va);

    log(level, buf);
}

void 
Logger::logEvent(const LogEvent& event)
{
    if (event.level() <= mThreshold) { 
        log(event.level(),event.message().c_str());
    }
}

} // namespace log
} // namespace arras

