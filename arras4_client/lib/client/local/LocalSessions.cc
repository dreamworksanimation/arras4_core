// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "LocalSessions.h"
#include "LocalSession.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

namespace arras4 {
    namespace client {

LocalSessions::LocalSessions(impl::ProcessManager& procMan) :
    mProcessManager(procMan)
{}


std::shared_ptr<LocalSession> 
LocalSessions::createSession(api::ObjectConstRef definition,
                             const std::string& aSessionId,
                             LocalSession::TerminateFunc tf)
{
    std::shared_ptr<LocalSession> sp = std::make_shared<LocalSession>(mProcessManager, aSessionId);
    sp->setDefinition(definition);
    sp->start(sp,tf);
    std::unique_lock<std::mutex> lock(mMutex);
    mSessions[sp->address().session] = sp;
    return sp;
}

std::shared_ptr<LocalSession>
LocalSessions::getSession(const api::UUID& sessionId)
{
    std::unique_lock<std::mutex> lock(mMutex);
    auto it = mSessions.find(sessionId);
    if (it != mSessions.end()) {
        return it->second;
    }
    return std::shared_ptr<LocalSession>();
}

void
LocalSessions::stopSession(const api::UUID& sessionId)
{
    std::shared_ptr<LocalSession> sp = getSession(sessionId);
    if (sp) sp->stop();
}

void
LocalSessions::pauseSession(const api::UUID& sessionId)
{
    std::shared_ptr<LocalSession> sp = getSession(sessionId);
    if (sp) sp->pause();
}

void
LocalSessions::resumeSession(const api::UUID& sessionId)
{
    std::shared_ptr<LocalSession> sp = getSession(sessionId);
    if (sp) sp->resume();
}

void
LocalSessions::abandonSession(const api::UUID& sessionId)
{
    std::shared_ptr<LocalSession> sp;
    {
	std::unique_lock<std::mutex> lock(mMutex);
	auto it = mSessions.find(sessionId);
	if (it != mSessions.end()) {
	    sp = it->second;
	}
    }
    if (sp) {
	sp->abandon();
    }
}

}
}
	
