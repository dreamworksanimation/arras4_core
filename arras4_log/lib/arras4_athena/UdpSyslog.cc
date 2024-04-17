// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "athena_platform.h"

#ifdef UDPSYSLOG_NO_BOOST
#include "UdpSyslog_noboost.cc"

#else
#include "UdpSyslog_boost.cc"

#endif

