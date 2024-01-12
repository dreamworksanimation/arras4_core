// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_COMPUTATION_ENVIRONMENT_IMPLH__
#define __ARRAS4_COMPUTATION_ENVIRONMENT_IMPLH__

#include "ComputationHandle.h"
#include "ControlMessageEndpoint.h"
#include "ComputationExitReason.h"

#include <chunking/ChunkingConfig.h>
#include <computation_api/ComputationEnvironment.h>
#include <shared_impl/MessageDispatcher.h>
#include <routing/ComputationMap.h>
#include <routing/Addresser.h>

namespace {
    // number of microseconds to elapse without incoming
    // messages before Computation's onIdle function is called.
    // (The function is called repeatedly at this interval while
    // the incoming queue remains idle : the interval however being
    // measured from the return of the previous onIdle to the call
    // to the next).
    constexpr long COMPUTATION_IDLE_INTERVAL = 40;
}

namespace arras4 {
    namespace impl {

class CompEnvironmentImpl : public api::ComputationEnvironment,
                            public MessageHandler,
                            public Controlled
{
public:

    // throws ComputationLoadError
    CompEnvironmentImpl(const std::string& name,
                        const std::string& dsoName,
                        const api::Address& address)
        : mName(name),
          mComputation(dsoName,this),
          mAddress(address),
          mDispatcher(dsoName, *this,
                      std::chrono::microseconds(COMPUTATION_IDLE_INTERVAL)),
          mGo(false)
        {}

    // you must set the routing before starting the computation, or
    // outgoing messages will not be sent. This can be done
    // immediately after construction : you can also call setRouting()
    // again after starting to modify the routing.
    // routing is the subobject named "routing" in the initial config
    // file passed to execComp. The same format is also used in
    // the 'update' ControlMessage
    bool setRouting(api::ObjectConstRef routing);

    // ComputationEnvironment interface
  
    api::Message send(const api::MessageContentConstPtr& content,
                      api::ObjectConstRef options);
    api::Object environment(const std::string& name);
    api::Result setEnvironment(const std::string& name, 
                          api::ObjectConstRef value);

    // MessageHandler interface deals with messages coming in
    // to the computation
    void handleMessage(const api::Message& message);
    void onIdle();

    // Controlled interface deals with control messages 
    // (e.g "go", "stop") intercepted by the ControlMessageEndpoint
    // mControlSource. data is valid for duration of call
    void controlMessage(const std::string& command,
	                const std::string& data);

    // initialize the computation. Log error and return 
    // api::Invalid if configuration fails
    api::Result initializeComputation(ExecutionLimits& limits,
                                      api::ObjectRef config);

    // set up message dispatch using the provided source,
    // optionally wait for 'go' signal, then start the dispatcher.
    // return when the dispatcher exits due to stop signal or
    // error.
    ComputationExitReason runComputation(MessageEndpoint& source,
                                         ExecutionLimits& limits,
                                         bool waitForGoSignal);

    void signalGo();
    void signalStop();
    void signalUpdate(const std::string& data);

private:

    void applyChunkingConfig(api::ObjectRef config);
    ComputationExitReason waitForGoSignal();

    std::string mName;
    ComputationHandle mComputation;
    api::Address mAddress;
    Addresser mAddresser;
    MessageDispatcher mDispatcher;

    mutable std::mutex mGoMutex;     
    std::condition_variable mGoCondition;
    bool mGo;

    ChunkingConfig mChunkingConfig;

};
        
}
}
#endif
