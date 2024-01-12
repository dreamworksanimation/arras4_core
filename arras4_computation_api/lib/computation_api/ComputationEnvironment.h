// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_COMPUTATION_ENVIRONMENTH__
#define __ARRAS4_COMPUTATION_ENVIRONMENTH__

#include <message_api/messageapi_types.h>
#include <message_api/Object.h>

#include <string>
#include <memory>
namespace arras4 {
    namespace api {

class Computation;

// Provides an environment for a computation.
class ComputationEnvironment 
{
public:
    virtual ~ComputationEnvironment() {}

    virtual Message send(const MessageContentConstPtr& content,
                         ObjectConstRef options)=0;

    virtual Object environment(const std::string& name)=0;
    virtual Result setEnvironment(const std::string& name, 
                                  ObjectConstRef value)=0;
};
        
}
}
#endif
