// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "CompEnvironmentImpl.h"
#include "PerformanceMonitor.h"

#include <chunking/ChunkingMessageEndpoint.h>
#include <core_messages/ControlMessage.h>
#include <message_api/MessageFormatError.h>
#include <computation_api/Computation.h>
#include <computation_api/version.h>
#include <computation_api/standard_names.h>

#include <message_impl/Envelope.h>
#include <shared_impl/MessageDispatcher.h>

#include <routing/Addresser.h>
#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <sstream>

namespace {
    // number of seconds to wait for a postGo before aborting the run
    constexpr long WAIT_FOR_GO_SECONDS = 600;
}

namespace arras4 {
    namespace impl {

  
bool 
CompEnvironmentImpl::setRouting(api::ObjectConstRef routing)
{ 
    // setup addressing from the routing member of a JSON session description
    // this can come from the initial session config (at startup) or from
    // an "update" ControlMessage once we are running
    api::ObjectConstRef mapObj = routing[mAddress.session.toString()]["computations"];
    if (mapObj.isNull() || !mapObj.isObject()) {
        ARRAS_ERROR(log::Id("badCompMap") << "Invalid computation map in routing data");
        return false;
    }
    ComputationMap computationMap(mAddress.session, mapObj);
 
    api::ObjectConstRef filterObj = routing["messageFilter"];
    if (filterObj.isNull() || !filterObj.isObject()) {
        ARRAS_ERROR(log::Id("badMessageFilter") << "Invalid message filter in routing data");
        return false;
    }
    mAddresser.update(mAddress.computation,
                      computationMap,
                      filterObj);
    return true;
}

api::Message CompEnvironmentImpl::send(const api::MessageContentConstPtr& content,
                                       api::ObjectConstRef options)
{
    Envelope envelope(content, options);
    api::ObjectConstRef to = options[api::MessageOptions::sendTo];
    if (to.isNull()) {
        mAddresser.address(envelope);
    } else {
        mAddresser.addressTo(envelope,to);
    }

    ARRAS_ATHENA_TRACE(2,"{trace:message} post " <<
                       envelope.metadata()->instanceId().toString() << " " <<
                       mAddress.computation.toString() << " " <<
                       envelope.metadata()->sourceId().toString() << " " <<
                       envelope.metadata()->routingName() << " " <<
                       content->classId().toString());

    // set trace flag if logger trace level >= 3
    // this will cause additional tracing as message is transported
    if (log::Logger::instance().traceThreshold() >= 3) {
        envelope.metadata()->trace() = true;
    }

    bool ok = mDispatcher.send(envelope);
    if (!ok) {
        ARRAS_ERROR(log::Id("sendFailed") <<
                    "Message send from computation failed for " << 
                    envelope.metadata()->routingName());
    }

    return envelope.makeMessage();
}

api::Object CompEnvironmentImpl::environment(const std::string& name)
{
    if (name == api::EnvNames::apiVersion)
        return api::Object(api::ARRAS4_COMPUTATION_API_VERSION);
    else if (name == api::EnvNames::computationName)
        return api::Object(mName);
    else if (name == "computation.address") {
        api::Object addr;
        mAddress.toObject(addr);
        return addr;
    }
    return api::Object();
}

api::Result CompEnvironmentImpl::setEnvironment(const std::string& /*name*/, 
                                                api::ObjectConstRef /*value*/)
{
    return api::Result::Unknown;
}
 
void CompEnvironmentImpl::handleMessage(const api::Message& message)
{
    api::Object instId = message.get(api::MessageData::instanceId);
    api::Object routingName = message.get(api::MessageData::routingName);
    ARRAS_ATHENA_TRACE(2,"{trace:message} dispatch " <<
                       (instId.isString() ? instId.asString() : "<error>") << " " <<
                       mAddress.computation.toString() << " " <<
                       (routingName.isString() ? routingName.asString() : "<error>"));

    api::Result result = mComputation->onMessage(message);

    ARRAS_ATHENA_TRACE(2,"{trace:message} handled " <<
                       (instId.isString() ? instId.asString() : "<error>") << " " <<
                       mAddress.computation.toString() << " " <<
                       (routingName.isString() ? routingName.asString() : "<error>") << " " <<
                       static_cast<int>(result));

    if (result == api::Result::Unknown) {
        ARRAS_WARN(log::Id("warnMessageIgnored") << "Computation ignored message: " << message.describe());
    } else if (result == api::Result::Invalid) {
        ARRAS_ERROR(log::Id("messageInvalid") << "Computation flagged message as invalid: " << message.describe());
        throw api::MessageFormatError("Computation::onMessage() returned 'Invalid'");
    }
}

void CompEnvironmentImpl::onIdle()
{
    mComputation->onIdle();
}

// startup procedure:
// 1) constructor       :       load computation, may throw ComputationLoadError
// 2) initializeComputation() : configure computation, api::Invalid => config failure
// 3) runComputation()        : ER_Quit => postStop or stop control message (i.e. normal exit)
//                              ER_GoTimeout => timed out waiting for go message
//                              ER_Disconnected => peer disconnected unexpectedly
//                              ER_MessageError => error in message transmission
//                                                 or serialization
//                              ER_HandlerError => Computation onMessage/onIdle flagged error


// initialize the computation and routing. Log error and return 
// api::Invalid if configuration fails
api::Result 
CompEnvironmentImpl::initializeComputation(ExecutionLimits& limits,
                                           api::ObjectRef config)
{
    applyChunkingConfig(config);
    // check if computation wants hyperthreading
    api::Object wantsHyperthreading = 
        mComputation->property(api::PropNames::wantsHyperthreading);
    if (!wantsHyperthreading.isBool() ||
        !wantsHyperthreading.asBool()) {
        limits.disableHyperthreading();
    }

    // add limit information to config
    config[api::ConfigNames::maxMemoryMB] = limits.maxMemoryMB();
    config[api::ConfigNames::maxThreads] = limits.maxThreads();

    // configure the computation
    api::Result res = mComputation->configure("initialize",config);
    if (res == api::Result::Invalid) {
        ARRAS_ERROR(log::Id("compConfigFailed") << 
                    "Configuration of the computation failed. Not starting execution.");
    }

    return res;
}

// allow config to modify default chunking settings
void
CompEnvironmentImpl::applyChunkingConfig(api::ObjectRef config)
{
    if (config["chunking"].isBool())
        mChunkingConfig.enabled = config["chunking"].asBool();
    size_t minChunkingSize = 0;
    if (config["minChunkingMb"].isIntegral()) 
        minChunkingSize = config["minChunkingMb"].asInt() * 1024  * 1024ull;
    if (config["minChunkingBytes"].isIntegral()) 
        minChunkingSize += config["minChunkingBytes"].asInt();
    if (minChunkingSize)
        mChunkingConfig.minChunkingSize = minChunkingSize;
    size_t chunkSize = 0;
    if (config["chunkSizeMb"].isIntegral()) 
        chunkSize = config["chunkSizeMb"].asInt() * 1024  * 1024ull;
    if (config["chunkSizeBytes"].isIntegral()) 
        chunkSize += config["chunkSizeBytes"].asInt();
    if (chunkSize)
        mChunkingConfig.chunkSize = chunkSize;
}
    
ComputationExitReason 
CompEnvironmentImpl::runComputation(MessageEndpoint& source,
                                    ExecutionLimits& limits,
                                    bool waitForGo)
{
    mGo = false;
    // will filter control messages, calling our controlMessage()
    // function instead of queuing/dispatching them
    ControlMessageEndpoint controlSource(source,*this);
    
    // legacy message chunking support
    std::shared_ptr<ChunkingMessageEndpoint> chunkingSource = 
        std::make_shared<ChunkingMessageEndpoint>(controlSource,mChunkingConfig);

    // start performance monitor thread
    api::Address to(mAddress.session, mAddress.node, api::UUID::null);
    api::AddressList toList;
    toList.push_back(to);
    PerformanceMonitor monitor(limits,mDispatcher,mAddress,toList);
                     
    std::thread monitorThread(&PerformanceMonitor::run, &monitor);

    // start queueing incoming messages
    mDispatcher.startQueueing(chunkingSource);

    // send a special "ready" message to node to announce our connection
    impl::Envelope envelope(new ControlMessage("ready"));
    envelope.to().push_back(to);
    envelope.metadata()->from() = mAddress;
    mDispatcher.send(envelope);
 
    ARRAS_ATHENA_TRACE(0,"{trace:comp} ready " <<
                       mAddress.computation.toString());

    // wait for someone to call signalGo
    if (waitForGo) {
        ARRAS_DEBUG("Computation is Waiting for a 'go' signal");
        ComputationExitReason er = waitForGoSignal();
        if (er != ComputationExitReason::None) {
            return er;
        }
    }

    bool configureStartException = false;
    try {
        mComputation->configure("start",api::Object());
    } catch (const std::exception& e) {
        ARRAS_ERROR("std::exception in configure(\"start\"): " <<  e.what());
        configureStartException = true;
    } catch (...) {
        ARRAS_ERROR("Unknown exception in configure(\"start\")");
        configureStartException = true;
    }
    ARRAS_ATHENA_TRACE(0,"{trace:comp} start " <<
                       mAddress.computation.toString());

    // begin dispatching messages
 
    if (configureStartException) {
        mDispatcher.postQuit();
    } else {
        try {
            mDispatcher.startDispatching(limits);
        } catch (const std::logic_error&) {
            // if the computation is shut down before being asked to start then the
            // the dispatcher can be Quit before reaching here so catch the
            // the exception for trying to startDispatch when it isn't Queueing.
        }
    }

    // wait for dispatcher to exit, due to error or a call to signalStop
    DispatcherExitReason der = mDispatcher.waitForExit();

    ARRAS_ATHENA_TRACE(0,"{trace:comp} stop " <<
                       mAddress.computation.toString());

    bool configureStopException = false;
    if (!configureStartException) {
        try {
            mComputation->configure("stop",api::Object());
        } catch (const std::exception& e) {
            ARRAS_ERROR("std::exception in configure(\"stop\"): " << e.what());
            configureStopException = true;
        } catch (...) {
            ARRAS_ERROR("Unknown exception in configure(\"stop\") ");
            configureStopException = true;
        }
    }

    // shut down performance monitor
    monitor.stop();
    if (monitorThread.joinable())
        monitorThread.join();

    if (configureStartException) {
        return ComputationExitReason::StartException;
    } else if (configureStopException) {
        return ComputationExitReason::StopException;
    } else {
        return dispatcherToComputationExitReason(der);
    }
}

ComputationExitReason CompEnvironmentImpl::waitForGoSignal()
{
    std::unique_lock<std::mutex> lock(mGoMutex);
    while (!mGo) {
        std::chrono::seconds sec(WAIT_FOR_GO_SECONDS);
        std::cv_status cvs = mGoCondition.wait_for(lock,sec);
        if (cvs == std::cv_status::timeout)
            return ComputationExitReason::Timeout;
    }
    return ComputationExitReason::None;
}

// Control messages are the messages like 'go' and 'stop' that
// actually control execution of the main run loop. To allow
// this they are intercepted by mControlSource without ever being
// passed on to the dispatcher, and come to this function instead.
void CompEnvironmentImpl::controlMessage(const std::string& command,
					 const std::string& data)
{
    if (command == "go") {
        signalGo();
    } else if (command == "stop") {
        signalStop();
    } else if (command == "abort") {
        signalStop(); // no different from "stop" at present
    } else if (command == "update") {
        signalUpdate(data);
    }
}

void CompEnvironmentImpl::signalGo()
{
    std::unique_lock<std::mutex> lock(mGoMutex);
    mGo = true;
    lock.unlock();
    mGoCondition.notify_all();
}

void CompEnvironmentImpl::signalStop()
{
    // make sure run() exits even in the case that
    // signalGo() hasn't been called yet...
    signalGo();
    // this will cause waitForExit() to terminate. If
    // waitForExit() hasn't been called yet, it
    // will terminate immediately when it _is_ called
    mDispatcher.postQuit();
}

void CompEnvironmentImpl::signalUpdate(const std::string& data)
{
    // update is provided as the "data" member of an update ControlMessage
    // currently it contains a single member "routing" with the same format as
    // the "routing" entry in the initial config passed to execComp
    // the object is valid until end of function
    api::Object dataObj;
    try {
        api::stringToObject(data,dataObj);
    } catch (api::ObjectFormatError& e) {
        ARRAS_ERROR(log::Id("badUpdateMessage") << 
                    "Invalid data in update ControlMessage : " << e.what()); 
    }
    api::ObjectConstRef routing = dataObj["routing"];
    if (routing.isObject() && !routing.isNull()) {
        setRouting(routing);
    } else {
        ARRAS_ERROR(log::Id("badUpdateMessage") << 
                "Invalid data in update ControlMessage : should contain 'routing' object.");
    }
}

}
}
