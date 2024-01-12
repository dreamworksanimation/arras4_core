// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "StreamLogger.h"
#include <assert.h>
#include <ostream>

namespace arras4 {
namespace log {

StreamLogger::StreamLogger(std::ostream& o)
    : mStrm(o)
{
}

StreamLogger::~StreamLogger()
{

}

void StreamLogger::log(Level /*level*/, const char *message)
{
    mStrm << message << std::endl;
}

}
}

