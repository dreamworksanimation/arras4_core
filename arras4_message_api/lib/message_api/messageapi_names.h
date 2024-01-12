// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef ARRAS4_MESSAGEAPI_NAMES_H_
#define ARRAS4_MESSAGEAPI_NAMES_H_

#include <string>

namespace arras4 {
    namespace api {


// names of standard message data, available through 'get'
struct MessageData {
    static const std::string instanceId;      // datatype string
    static const std::string sourceId;        // datatype string
    static const std::string creationTimeSecs;        // datatype int
    static const std::string creationTimeMicroSecs;   // datatype int
    static const std::string creationTimeString;      // datatype string
    static const std::string from;          // datatype Address
    static const std::string routingName;     // datatype string
};

// names of standard message data, available to be set when sending
struct MessageOptions {
    // set message source id
    static const std::string sourceId;       // datatype string
    static const std::string routingName;    // datatype string
    static const std::string sendTo;         // datatype Address
};

}
}
#endif
