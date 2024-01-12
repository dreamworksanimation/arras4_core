// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef ARRAS4_EXEC_COMP_H_
#define ARRAS4_EXEC_COMP_H_



#include <network/network_types.h>
#include <message_api/messageapi_types.h>
#include <message_api/Object.h>

#include <string>
#include <memory>

namespace arras4 {

    namespace log {
        class AthenaLogger;
    }

    namespace impl {

    class ExecutionLimits;

class ExecComp {
public:
    ExecComp(ExecutionLimits& limits,
             api::ObjectRef config,
             log::AthenaLogger& logger) :
        mLimits(limits),
        mConfig(config),
        mLogger(logger)
        {}

    int run();

private:

    std::unique_ptr<arras4::network::IPCSocketPeer>
    connectToServer(const api::Address& compAddr,
                    const std::string& ipcAddr);

    ExecutionLimits& mLimits;
    api::ObjectRef mConfig;
    log::AthenaLogger& mLogger;
};

}
}
#endif
