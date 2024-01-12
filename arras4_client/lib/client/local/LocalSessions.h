// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_LOCAL_SESSIONS_H__
#define __ARRAS4_LOCAL_SESSIONS_H__

#include "LocalSession.h"

#include <message_api/Object.h>
#include <message_api/UUID.h>
#include <execute/ProcessManager.h>

#include <map>
#include <memory>
#include <mutex>

namespace arras4 {
    namespace client {

class LocalSessions
{
public:
    LocalSessions(impl::ProcessManager& procMan);

    std::shared_ptr<LocalSession> createSession(api::ObjectConstRef definition,
                                                const std::string& aSessionId,
						LocalSession::TerminateFunc tf);
    void stopSession(const api::UUID& sessionId);
    void pauseSession(const api::UUID& sessionId);
    void resumeSession(const api::UUID& sessionId);

    // detach callbacks immediately : used during shutdown of client
    void abandonSession(const api::UUID& sessionId);



private:

    std::shared_ptr<LocalSession> getSession(const api::UUID& sessionId);
    
    impl::ProcessManager& mProcessManager;

    std::map<api::UUID,std::shared_ptr<LocalSession>> mSessions;
    std::mutex mMutex;

};

}
}
#endif
