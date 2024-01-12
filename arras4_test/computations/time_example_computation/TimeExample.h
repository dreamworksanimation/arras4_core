// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef TIMEEXAMPLE_H
#define TIMEEXAMPLE_H

// needed for arras4::api:Computation
#include <computation_api/Computation.h>

// needed for arras4::api::ObjectConstRef
#include <message_api/Object.h>

// needed for arras4::api::Result
#include <message_api/messageapi_types.h>

#include <string>


namespace arras4 {
namespace time_example {

class TimeExample : public arras4::api::Computation
{
public:
    TimeExample(arras4::api::ComputationEnvironment* env) :
        arras4::api::Computation(env) {}
    
protected:
    arras4::api::Result configure(const std::string& op,
                                  arras4::api::ObjectConstRef& config);
    
    arras4::api::Result onMessage(const arras4::api::Message& aMsg);

private:
};

} // end namespace time_example
} // end namespace arras4


#endif // TIMEEXAMPLE_H
