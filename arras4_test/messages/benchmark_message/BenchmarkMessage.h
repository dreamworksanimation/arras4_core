// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_TEST_MESSAGEH__
#define __ARRAS4_TEST_MESSAGEH__

#include <message_api/ContentMacros.h>
#include <string>

namespace arras4 {
    namespace benchmark {


class BenchmarkMessage : public arras4::api::ObjectContent
{
public:
    enum MessageType {
        NOOP,
        ACK,                    // this is an acknowledge message
        SEND_ACK,               // Request an acknowledge message
        START_STREAM_OUT,       // start stream to client or another computation if it isn't already running
        SEND_REPORT,            // send data about the test which is running
        REPORT,                 // this is data about the test which is running
        STOP,                   // stop everything
        LOGSPEED,               // run a logging speed test
        PRINTENV};              // send all of the environment variables back to the requester

    ARRAS_CONTENT_CLASS(BenchmarkMessage,"8b6f0270-ffb9-419f-9c3e-4a15d8d67598",0);
    
    BenchmarkMessage();
    BenchmarkMessage(MessageType type, const std::string& value=std::string(), const std::string& from=std::string());
    ~BenchmarkMessage();

    void serialize(arras4::api::DataOutStream& to) const;
    void deserialize(arras4::api::DataInStream& from, unsigned version);

    MessageType mType;
    std::string mFrom;
    std::string mValue;
};

}
}
#endif
