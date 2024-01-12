// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_STREAMLOGGER_H__
#define __ARRAS_STREAMLOGGER_H__

#include "Logger.h"
#include <iosfwd>

namespace arras4 {
namespace log {
/**
 * \brief Writes logs to a C++ std::ostream
 */
class StreamLogger : public Logger
{
public:
    StreamLogger(std::ostream& o);
    ~StreamLogger();

protected:
    void log(Level level, const char *message);
    std::ostream& mStrm;
};

}
}

#endif // __ARRAS_STREAMLOGGER_H__

