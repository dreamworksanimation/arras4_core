// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef TIMEEXAMPLEMESSAGE_H
#define TIMEEXAMPLEMESSAGE_H

// needed for ARRAS_CONTENT_CLASS macro and
// arras4::api::ObjectContent class definition
#include <message_api/ContentMacros.h>

#include <string>

//
// Message for TimeExample computation, contains the time query
//

    class TimeExampleMessage : public arras4::api::ObjectContent
    {
    public:
        ARRAS_CONTENT_CLASS(
            // the name of this class
            TimeExampleMessage,

            // the UUID for the message type. This must be different for
            // every message type.
            // One can be generated using /usr/bin/uuidgen
            "1feb0087-0e31-4eeb-a6d2-1a8180d26d41",

            // The version of the message type. To allow receivers to be run
            // with older senders this should be incremented every time the
            // message format changes. Otherwise compatibility between the
            // sender and receiver need to be managed with an alterative
            // like rez.
            0);

        ~TimeExampleMessage() {};

        // Message implementation, replacing the base class virtual functions
        void serialize(arras4::api::DataOutStream& out) const;
        void deserialize(arras4::api::DataInStream& in, unsigned version);

        // extra member functions unique to TestExampleMessage
        const std::string& getValue() const;
        void setValue(const std::string& aCommand);
        void dump(std::ostream& aStrm) const;

    private:
        std::string mValue;
    };

#endif // TIMEEXAMPLEMESSAGE_H

