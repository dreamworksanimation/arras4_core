// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_RES_RESOLVE_H__
#define __ARRAS4_RES_RESOLVE_H__

#include <message_api/Object.h>

#include <string>

namespace arras4 {
    namespace impl {
	class ProcessManager;
    }

namespace client {

    class SessionDefinition;

std::string rezResolve(impl::ProcessManager& procMan,
		       api::ObjectRef rezSettings,
                       std::string& err);

bool rezResolve(impl::ProcessManager& procMan,
		SessionDefinition& def,
		std::string& err);

}
}
#endif
