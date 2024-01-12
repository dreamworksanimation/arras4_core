// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_COMPUTATIONH__
#define __ARRAS4_COMPUTATIONH__

#include "ComputationEnvironment.h"
#include "standard_names.h"

#include <message_api/Message.h>
#include <message_api/messageapi_names.h>

#include <string>
#include <memory>

namespace arras4 {
    namespace api {

class Computation 
{
public:
    Computation(ComputationEnvironment* env) :
        mEnv(env) {}

    virtual ~Computation() {}

    virtual Result onMessage(const Message& message)=0;
    virtual void onIdle() {}

    virtual Result configure(const std::string& op,
                             ObjectConstRef config)=0;
    virtual Object property(const std::string& name);

    Message send(const MessageContentConstPtr& content,
                 ObjectConstRef options=Object())
        { return mEnv->send(content,options); }

    Message send(const MessageContent* content,
                    ObjectConstRef options=Object())
        {  return mEnv->send(MessageContentConstPtr(content),options); }
              
    Object environment(const std::string& name)
    { return mEnv->environment(name); }
    Result setEnvironment(const std::string& name, 
                          ObjectConstRef value)
    { return mEnv->setEnvironment(name,value); }

private:
    ComputationEnvironment* mEnv;
};


inline Object withSource(ObjectConstRef sourceId) {
    Object ret;
    ret[MessageOptions::sourceId] = sourceId;
    return ret;
}
inline Object withSource(const Message& msg) {
    return withSource(msg.get(MessageData::sourceId));
}
inline Object withSource(const std::string& sourceId) {
    Object ret;
    ret[MessageOptions::sourceId] = sourceId;
    return ret;
}
inline Object sendTo(ObjectConstRef address) {
    Object ret;
    ret[MessageOptions::sendTo] = address;
    return ret;
}
    

#define COMPUTATION_CREATE_FUNC "_create_computation"
#define COMPUTATION_CREATOR(COMPNAME)                                   \
        extern "C" {                                                    \
            ::arras4::api::Computation*                                 \
            _create_computation(                                        \
                ::arras4::api::ComputationEnvironment* env) {           \
                return new COMPNAME(env);                               \
            } \
        }

        

}
}
#endif






