// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_LOGEVENTSTREAM_H__
#define __ARRAS_LOGEVENTSTREAM_H__

#include "Logger.h"

namespace arras4 {
    namespace log {

template<typename T>
inline LogEvent& operator<<(LogEvent& evt,const T& t) { evt.messageStream() << t; return evt; }

struct Id { 
    Id(const std::string& s) : mId(s) {}
    std::string mId;
};
inline LogEvent& operator<<(LogEvent& evt,const Id& x) {
    evt.id() = x.mId; return evt;
}

struct Session { 
    Session(const std::string& s) : mSession(s) {}
    std::string mSession;
};
inline LogEvent& operator<<(LogEvent& evt,const Session& x) {
    evt.session() = x.mSession; return evt;
}

struct Module { 
    Module(const std::string& s) : mModule(s) {}
    std::string mModule;
};
inline LogEvent& operator<<(LogEvent& evt,const Module& x) {
    evt.module() = x.mModule; return evt;
}

extern Module Arras;

}
}

#define ALOG(LEVEL, M) \
{ ::arras4::log::LogEvent _evt(LEVEL); \
    _evt << M;                                      \
    ::arras4::log::Logger::instance().logEvent(_evt); }

#define ALOG_FATAL(M) ALOG(::arras4::log::Logger::LOG_FATAL, M);
#define ALOG_ERROR(M) ALOG(::arras4::log::Logger::LOG_ERROR, M);
#define ALOG_WARN(M) ALOG(::arras4::log::Logger::LOG_WARN, M);
#define ALOG_INFO(M) ALOG(::arras4::log::Logger::LOG_INFO, M);
#define ALOG_DEBUG(M) ALOG(::arras4::log::Logger::LOG_DEBUG, M);
#define ATHENA_TRACE(L,M) ALOG((::arras4::log::Logger::Level)((L)+::arras4::log::Logger::ATHENA_TRACE0), M);

#define ARRAS_FATAL(M) ALOG_FATAL(::arras4::log::Arras << M)
#define ARRAS_ERROR(M) ALOG_ERROR(::arras4::log::Arras << M)
#define ARRAS_WARN(M) ALOG_WARN(::arras4::log::Arras << M)
#define ARRAS_INFO(M) ALOG_INFO(::arras4::log::Arras << M)
#define ARRAS_DEBUG(M) ALOG_DEBUG(::arras4::log::Arras << M)
#define ARRAS_ATHENA_TRACE(L,M) ATHENA_TRACE(L,::arras4::log::Arras << M)

#if defined(DEBUG)
    #define ALOG_TRACE(M) ALOG(::arras4::log::Logger::LOG_TRACE, M);
    #define ARRAS_TRACE(M) ALOG_TRACE(::arras4::log::Arras << M);
#else // DEBUG
    #define ALOG_TRACE(M)
    #define ARRAS_TRACE(M)
#endif // DEBUG
#endif

