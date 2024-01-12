// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CLIENT_COMPONENT_H__
#define __ARRAS4_CLIENT_COMPONENT_H__

#include <message_api/messageapi_types.h>

namespace arras4 {
    namespace client {

        class Client;

        class Component {
        public:
            // takes reference to an existing arras::Client instance
            Component(Client& aClient);

            // because compiler will complain otherwise...
            virtual ~Component();

            // subclasses should override this if they want to be notified of
            // message traffic from the Arras service. Default implementation
            // ignores incoming messages
            virtual void onMessage(const api::Message& aMsg);

            // subclasses should override this if they want to be notified of
            // SessionStatusMessage frp, tje Arras service. Default implementation
            // prints the message to the INFO log
            virtual void onStatusMessage(const api::Message& aMsg);

            // subclasses should override this is they want to be notified when
            // the engineReady message is received
            virtual void onEngineReady();

        protected:
            // call this to send a message to the Arras service
            void send(const api::MessageContentConstPtr& content);

        private:
            Client& mClient;
        };

    } // namespace client
} // namespace arras4

#endif // __ARRAS_CLIENT_COMPONENT_H__

