// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_SESSIONNOTIFY_H__
#define __ARRAS4_SESSIONNOTIFY_H__

#include <message_api/Object.h>
#include <string>
#include <thread>

namespace arras4 {
namespace client {

class SessionNotification {
  public:
    SessionNotification(
        api::ObjectConstRef aDefinition,
        api::ObjectConstRef aSessionOptions,
        const std::string& aSessionId,
        const std::string& aUserAgent);

  private:
    void registerThread();
    void updateCores(api::ObjectRef& aDefinition);
    void registerSession();
    std::string getCoordinatorUrl();


    api::Object mDefinition;
    api::Object mSessionOptions;
    std::string mSessionId;
    std::string mUserAgent;
    std::thread mThread;
};

} // end namespace client
} // end namespace arras4

#endif // __ARRAS4_SESSIONNOTIFY_H__

