// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_STANDALONE_ENVIRONMENT_H__
#define __ARRAS4_STANDALONE_ENVIRONMENT_H__

#include <computation_api/ComputationEnvironment.h>

#include <functional>
#include <exception>
#include <string>

namespace arras4 {
    namespace impl {

class LoadError : public std::exception
{
public:
    LoadError(const std::string& aDetail) : mMsg(aDetail) {}
    ~LoadError() {}
    const char* what() const throw() { return mMsg.c_str(); }

protected:
    std::string mMsg;
};

typedef std::function<api::Computation*(api::ComputationEnvironment*)> ComputationFactoryFunction;
typedef std::function<void(const api::Message& message)> OnMessageFunction;

class StandaloneEnvironment : public api::ComputationEnvironment
{
public:

    // throws LoadError
    StandaloneEnvironment(const std::string& name,
                          const std::string& dsoName,
                          OnMessageFunction omf);

    StandaloneEnvironment(const std::string& name,
                          ComputationFactoryFunction cff,
                          OnMessageFunction omf);
    
    ~StandaloneEnvironment();
    
    // ComputationEnvironment interface  
    api::Message send(const api::MessageContentConstPtr& content,
                      api::ObjectConstRef options);
    api::Object environment(const std::string& name);
    api::Result setEnvironment(const std::string& name, 
                          api::ObjectConstRef value);

    // Interact with computation
    api::Result initializeComputation(api::ObjectRef config);
    api::Result sendMessage(const api::MessageContentConstPtr& content,
                            api::ObjectConstRef options=api::Object());
    void performIdle();
    void shutdownComputation();
    

private:
    void loadDso(const std::string& dsoName);

    std::string mName;
    api::Computation* mComputation;
    OnMessageFunction mMessageFunction;
    void* mDso;

};
        
}
}
#endif
