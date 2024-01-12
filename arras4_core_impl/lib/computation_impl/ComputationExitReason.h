// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_COMPUTATION_EXIT_REASONH__
#define __ARRAS4_COMPUTATION_EXIT_REASONH__

#include <shared_impl/DispatcherExitReason.h>
#include <string>


namespace {
    std::string CERS_StateError("trying to run from an invalid starting state");
    std::string CERS_Timeout("timed out waiting for computation start signal");
    std::string CERS_StartException("exception in computation(start)");
    std::string CERS_StopException("exception in computation(stop)");
    std::string CERS_Unknown("unknown exit reason");
}


namespace arras4 {
    namespace impl {

// The possible reasons a computation might stop running.
// Many of these are the same reasons why the dispatcher might stop
enum class ComputationExitReason {
    None = (int)DispatcherExitReason::None,    
        Quit = (int)DispatcherExitReason::Quit, 
        Disconnected = (int)DispatcherExitReason::Disconnected, 
        MessageError = (int)DispatcherExitReason::MessageError, 
        HandlerError =  (int)DispatcherExitReason::HandlerError, 
    StateError,    // tried to run from incorrect state (i.e. not INITIALIZED)
    Timeout,     // timed out waiting for 'go'
    StartException, // Got at exception in computation(start)
    StopException, // Got at exception in computation(stop)
};

inline const std::string& computationExitReasonAsString(ComputationExitReason cer)
{
    switch (cer) {
    case ComputationExitReason::None: return dispatcherExitReasonAsString(DispatcherExitReason::None);
    case ComputationExitReason::Quit: return dispatcherExitReasonAsString(DispatcherExitReason::Quit);
    case ComputationExitReason::Disconnected: return dispatcherExitReasonAsString(DispatcherExitReason::Disconnected);
    case ComputationExitReason::MessageError: return dispatcherExitReasonAsString(DispatcherExitReason::MessageError);
    case ComputationExitReason::HandlerError: return dispatcherExitReasonAsString(DispatcherExitReason::HandlerError);
    case ComputationExitReason::StateError:   return CERS_StateError;  
    case ComputationExitReason::Timeout:    return CERS_Timeout;
    case ComputationExitReason::StartException: return CERS_StartException;
    case ComputationExitReason::StopException:  return CERS_StopException;
    default:
        return CERS_Unknown;
    }
}

inline ComputationExitReason dispatcherToComputationExitReason(DispatcherExitReason der)
{
    return (ComputationExitReason)der;
}

}
}
#endif
