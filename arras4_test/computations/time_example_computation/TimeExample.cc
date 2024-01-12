// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "TimeExample.h"

// needed for logging macros
#include <arras4_log/LogEventStream.h>
#include <memory>

// These are needed by already included by TimeExample.h
// #include <message_api/Object.h>
// #include <message_api/messageapi_types.h>

#include <string>
#include <time.h>

// This example receives and sends TimeExampleMessage messages
#include <time_example_message/TimeExampleMessage.h>


// These are needed but already included by TimeExample.h
// #include <message_api/Object.h>
// #include <message_api/messageapi_types.h>

namespace arras4 {
namespace time_example {

COMPUTATION_CREATOR(TimeExample);

arras4::api::Result
TimeExample::configure(const std::string& op,
                   arras4::api::ObjectConstRef& config)
{
    if (op == "start") {

        // nothing to do on start
        ARRAS_INFO("TimeExample::configure start: time to get to work");
        return arras4::api::Result::Success;
    } else if (op == "stop") {

        // nothing to do on stop
        ARRAS_INFO("TimeExample::configure start: time to stop running threads and destroy heap objects");
        return arras4::api::Result::Success;

    } else if (op == "initialize") {

        std::string configAsString = arras4::api::objectToString(config);
        std::string dsoName = config[api::ConfigNames::dsoName].asString();
        ARRAS_INFO("TimeExample::configure(): dsoName = " << dsoName);
        ARRAS_INFO("TimeExample::configure(): " << dsoName << " " << "maxThreads " << config[api::ConfigNames::maxThreads].asInt());
        ARRAS_INFO("TimeExample::configure(): " << dsoName << " " << "maxMemoryMB " << config[api::ConfigNames::maxMemoryMB].asInt());
        ARRAS_INFO("TimeExample::configure(): " << dsoName << " " << "my_session_parameter " << config["my_session_parameter"].asInt());
        ARRAS_INFO("TimeExample::configure(): - Incoming config: " << config[api::ConfigNames::dsoName].asString() << "/" << configAsString);
        return arras4::api::Result::Success;

    } else {
        // unknown configure op
        return arras4::api::Result::Unknown;
    }
}

arras4::api::Result
TimeExample::onMessage(const api::Message& aMsg)
{
    ARRAS_INFO("TimeExample::onMessage - Received a message: ");

    if ((aMsg.classId() == TimeExampleMessage::ID) && (aMsg.classVersion()== TimeExampleMessage::VERSION_NUM)) {
        TimeExampleMessage::ConstPtr timeExample = aMsg.contentAs<TimeExampleMessage>();

        std::string command = timeExample->getValue();
        ARRAS_INFO("TimeExample::onMessage - Received CRM: " << command);

        // get the current time
        time_t now = time(0);

        // convert time_t to tm
        struct tm tmstruct;
        localtime_r(&now, &tmstruct);

        // convert to a human readable string
        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tmstruct);

        // send the response
        TimeExampleMessage::Ptr response = std::make_shared<TimeExampleMessage>();
        response->setValue(timeExample->getValue() + " " + buf);
        send(response);

    } else {
        ARRAS_ERROR("TimeExample::onMessage - Ignoring message with id of: " << aMsg.classId().toString() <<
                    "and version of " << aMsg.classVersion());
        return arras4::api::Result::Unknown;
    }

    return arras4::api::Result::Success;
}

} // namespace time_example
} // namespace arras
