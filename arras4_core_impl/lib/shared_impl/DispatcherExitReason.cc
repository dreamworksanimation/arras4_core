// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "DispatcherExitReason.h"

namespace {
    std::string DERS_None("dispatcher still running");
    std::string DERS_Quit("requested to exit");
    std::string DERS_Disconnected("socket was disconnected");
    std::string DERS_MessageError("error transporting a message"); 
    std::string DERS_HandlerError("error handling a message"); 
    std::string DERS_Unknown("unknown error");
}


namespace arras4 {
    namespace impl {

const std::string& dispatcherExitReasonAsString(DispatcherExitReason der)
{
    switch (der) {
    case DispatcherExitReason::None:         return DERS_None;
    case DispatcherExitReason::Quit:         return DERS_Quit;
    case DispatcherExitReason::Disconnected: return DERS_Disconnected;
    case DispatcherExitReason::MessageError: return DERS_MessageError;  
    case DispatcherExitReason::HandlerError: return DERS_HandlerError;   
    default:
        return DERS_Unknown;
    }
}

}
}
