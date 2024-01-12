// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "messageapi_names.h"

namespace arras4 {
    namespace api {

const std::string MessageData::instanceId        = "instanceId";
const std::string MessageData::sourceId          = "sourceId";  
const std::string MessageData::creationTimeSecs  = "creationTimeSecs";
const std::string MessageData::creationTimeMicroSecs  = "creationTimeMicroSecs";
const std::string MessageData::creationTimeString  = "creationTimeString";
const std::string MessageData::from  = "from";
const std::string MessageData::routingName  = "routingName";

const std::string MessageOptions::sourceId       = "sourceId";       // datatype string
const std::string MessageOptions::routingName    = "routingName";    // datatype string
const std::string MessageOptions::sendTo    = "sendTo";    // datatype Address or array of Address
}
}
