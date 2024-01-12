// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ExecComp.h"


#include <message_api/Address.h>
#include <message_impl/messaging_version.h>
#include <message_impl/PeerMessageEndpoint.h>
#include <routing/ComputationMap.h>
#include <routing/Addresser.h>
#include <computation_impl/CompEnvironmentImpl.h>
#include <computation_impl/ComputationLoadError.h>
#include <shared_impl/RegistrationData.h>
#include <shared_impl/ProcessExitCodes.h>

#include <network/IPCSocketPeer.h>

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>
#include <arras4_athena/AthenaLogger.h>

#include <fstream>

#if defined(JSONCPP_VERSION_MAJOR)
#define memberName name
#endif

using namespace arras4::log;
using namespace arras4::api;

namespace arras4 {
    namespace impl {

// sets up and then runs a computation
// returns an exit code indicating what happened
// (EXIT_NORMAL or an error code)
int ExecComp::run()
{

    try {

        // 1) pull out the configuration info required to create the
        //    computation environment

        // log level
        Logger::Level logLevel = Logger::LOG_ERROR;
        if (mConfig["logLevel"].isIntegral()) {
            int level = mConfig["logLevel"].asInt();
            if (level >= Logger::LOG_FATAL && level <= Logger::LOG_TRACE) {
                logLevel = static_cast<Logger::Level>(level);
            }
        }
        mLogger.setThreshold(logLevel);
	if (mConfig["consoleLogStyle"].isIntegral()) {
	    int style = mConfig["consoleLogStyle"].asInt();
	    mLogger.setConsoleStyle(static_cast<ConsoleLogStyle>(style));
	}

        // computation config
        // ("config" is an object with only one member allowed)
        if (!mConfig["config"].isObject() ||
            !(mConfig["config"].size() == 1)) {
            ARRAS_ERROR(Id("badCompConfig") << "Invalid computation configuration");
            return ProcessExitCodes::INVALID_CONFIG_DATA;
        }
        api::ObjectIterator oIt = mConfig["config"].begin();
        std::string computationName = oIt.memberName(); // Json DEPRECATED
        api::ObjectRef computationConfig = *oIt;

        // save config if requested
        if (computationConfig["saveConfigTo"].isString()) {
            std::string filepath = computationConfig["saveConfigTo"].asString();
            api::Object toSave;
            toSave["config"] = computationConfig;
            mLimits.toObject(toSave["limits"]);
            std::string str = objectToStyledString(toSave);
            std::ofstream ofs(filepath);
            if (!ofs)
                ARRAS_WARN(Id("saveConfigFailed") << "Failed to save config to " << filepath);
            ofs << str;
            if (!ofs)
                ARRAS_WARN(Id("saveConfigFailed") << "Failed to save config to " << filepath);
            ofs.close();
        }

        // tracing
        int traceLevel = 0;
        if (computationConfig["traceThreshold"].isIntegral()) {
            traceLevel = computationConfig["traceThreshold"].asInt();
        } else if (mConfig["traceThreshold"].isIntegral()) {
            traceLevel =  mConfig["traceThreshold"].asInt();
        }
        mLogger.setTraceThreshold(traceLevel);

        // dso name
        if (!computationConfig["dso"].isString()) {
            ARRAS_ERROR(Id("missingDsoName") << "No DSO name provided");
            return ProcessExitCodes::INVALID_CONFIG_DATA;
        }
        std::string dsoName = computationConfig["dso"].asString();

        ARRAS_DEBUG("Starting computation " << computationName << " : "  << dsoName);
        mLogger.setProcessName("comp-"+computationName);

        // computation address
        Address computationAddress;
        if (!mConfig["compId"].isString()) {
            ARRAS_ERROR(Id("missingCompId") << "No computation ID provided");
            return ProcessExitCodes::INVALID_CONFIG_DATA;
        }
        computationAddress.computation = UUID(mConfig["compId"].asString());
        if (!mConfig["nodeId"].isString()) {
            ARRAS_ERROR(Id("missingNodeId") << "No node ID provided");
            return ProcessExitCodes::INVALID_CONFIG_DATA;
        }
        computationAddress.node = UUID(mConfig["nodeId"].asString());
        if (!mConfig["sessionId"].isString()) {
            ARRAS_ERROR(Id("missingSessionId") << "No session ID provided");
            return ProcessExitCodes::INVALID_CONFIG_DATA;
        }
        const std::string sessionIdString = mConfig["sessionId"].asString();
        computationAddress.session = UUID(sessionIdString);
 
        // IPC address
        if (!mConfig["ipc"].isString()) {
            ARRAS_ERROR(Id("missingIPCAddr") << "No IPC address provided");
            return ProcessExitCodes::INVALID_CONFIG_DATA;
        }
        const std::string ipcAddr = mConfig["ipc"].asString();

        // fetch routing data
        ObjectConstRef routing = mConfig["routing"];
        if (!routing.isObject() || routing.isNull()) {
            ARRAS_ERROR(log::Id("missingRoutingData") <<
                        "Invalid data in computation config : should contain 'routing' object.");
	    return ProcessExitCodes::INVALID_CONFIG_DATA;
	}

	// save routing if requested
        if (computationConfig["saveRoutingTo"].isString()) {
            std::string filepath = computationConfig["saveRoutingTo"].asString();
            std::string str = objectToStyledString(routing);
            std::ofstream ofs(filepath);
            if (!ofs)
                ARRAS_WARN(Id("saveRoutingFailed") << "Failed to save routing to " << filepath);
            ofs << str;
            if (!ofs)
                ARRAS_WARN(Id("saveRoutingFailed") << "Failed to save routing to " << filepath);
            ofs.close();
         }

        // 2) Create and initialize the environment
        CompEnvironmentImpl env(computationName,dsoName,computationAddress);
	if (!env.setRouting(routing)) {
            return ProcessExitCodes::INVALID_CONFIG_DATA;
        }

        ARRAS_DEBUG("Initializing computation " << computationName);
        Result res = env.initializeComputation(mLimits,computationConfig);
        if (res == Result::Invalid) {
            ARRAS_ERROR(Id("initCompFailed") << "Failed to initialize the computation");
            return ProcessExitCodes::INITIALIZATION_FAILED;
        }

        // 3) connect to the server, which also notifies 'node' that
        //    the computation is ready to run

        ARRAS_DEBUG("Connecting to node");
        std::unique_ptr<arras4::network::IPCSocketPeer> peer = connectToServer(computationAddress,ipcAddr);
        std::string traceInfo("C:"+ computationAddress.computation.toString() +
                              " N:"+computationAddress.node.toString());
        PeerMessageEndpoint endpoint(*peer,true,traceInfo);


        // 4) turn on autosave of messages if requested
        if (computationConfig["saveIncomingTo"].isString()) {
            endpoint.reader().enableAutosave(computationConfig["saveIncomingTo"].asString());
        }
        if (computationConfig["saveOutgoingTo"].isString()) {
            endpoint.writer().enableAutosave(computationConfig["saveOutgoingTo"].asString());
        }


        // 5) run the computation

        ARRAS_DEBUG("Running computation " << computationName);
        ComputationExitReason er = env.runComputation(endpoint,mLimits,true);

        // 5) computation has exited

        ARRAS_DEBUG("Computation " << computationName << " terminated : " << computationExitReasonAsString(er));

        if (er == ComputationExitReason::None ||
            er == ComputationExitReason::Quit)
            return ProcessExitCodes::NORMAL;

        // map ER codes to exit codes
        if (er == ComputationExitReason::Disconnected)
            return ProcessExitCodes::DISCONNECTED;
        if (er == ComputationExitReason::MessageError ||
            er == ComputationExitReason::HandlerError ||
            er == ComputationExitReason::StateError)
            return ProcessExitCodes::INTERNAL_ERROR;
        if (er == ComputationExitReason::Timeout)
            return ProcessExitCodes::COMPUTATION_GO_TIMEOUT;
        if (er == ComputationExitReason::StartException ||
            er == ComputationExitReason::StopException)
            return ProcessExitCodes::EXCEPTION_CAUGHT;

        return ProcessExitCodes::UNSPECIFIED_ERROR;

    }
    catch (ComputationLoadError& cle) {
        ARRAS_ERROR(Id("compLoadError") << "Computation failed to load : " << cle.what());
        return ProcessExitCodes::COMPUTATION_LOAD_ERROR;
    }
    catch (std::exception& e) {
        // this covers a number of arras_common/network peer errors...
        ARRAS_ERROR(Id("compException") << "Exception thrown running computation : " << e.what());
        return ProcessExitCodes::EXCEPTION_CAUGHT;
    }
    catch (...) {
        ARRAS_ERROR(Id("compException") << "Non-standard exception thrown running computation");
        return ProcessExitCodes::EXCEPTION_CAUGHT;
    }
}

// may throw arras_common exceptions such as PeerException...
std::unique_ptr<arras4::network::IPCSocketPeer>
ExecComp::connectToServer(const Address& compAddr,
                          const std::string& ipcAddr)
{

    RegistrationData regData(ARRAS_MESSAGING_API_VERSION_MAJOR,
                             ARRAS_MESSAGING_API_VERSION_MINOR,
                             ARRAS_MESSAGING_API_VERSION_PATCH);
    regData.mType = REGISTRATION_EXECUTOR;

    regData.mComputationId = compAddr.computation;
    regData.mNodeId = compAddr.node;
    regData.mSessionId = compAddr.session;

    std::unique_ptr<arras4::network::IPCSocketPeer> ipcSocketPeer(new arras4::network::IPCSocketPeer);
    ipcSocketPeer->connect(ipcAddr);

    ipcSocketPeer->send_or_throw(&regData, sizeof(regData), "execComp::start");
    return ipcSocketPeer;
}

}
}
