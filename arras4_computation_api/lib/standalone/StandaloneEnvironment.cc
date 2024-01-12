// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "StandaloneEnvironment.h"
#include "StandaloneMetadata.h"

#include <computation_api/version.h>
#include <computation_api/Computation.h>

#include <dlfcn.h>

namespace {
std::string safeDlerror() {
    char* dle = dlerror();
    if (dle)
        return dle;
    return "No dynamic loading error was reported";
}
}

namespace arras4 {
    namespace impl {


StandaloneEnvironment::StandaloneEnvironment(const std::string& name,
                const std::string& dsoName,
                OnMessageFunction omf) :
    mName(name), mMessageFunction(omf), mDso(nullptr)
{
    loadDso(dsoName);
}
    

StandaloneEnvironment::StandaloneEnvironment(const std::string& name,
                ComputationFactoryFunction cff,
                OnMessageFunction omf) :
    mName(name), mMessageFunction(omf), mDso(nullptr)
{
    mComputation = cff(this);
}

StandaloneEnvironment::~StandaloneEnvironment()
{
    delete mComputation;
    if (mDso) dlclose(mDso);
    mDso = nullptr;
}

api::Message StandaloneEnvironment::send(const api::MessageContentConstPtr& content,
                                   api::ObjectConstRef options)
{
    StandaloneMetadata::Ptr md(new StandaloneMetadata(content,options));
    api::Message msg(md,content);
    mMessageFunction(msg);
    return msg;
}

api::Object StandaloneEnvironment::environment(const std::string& name)
{
    if (name == api::EnvNames::apiVersion)
        return api::Object(api::ARRAS4_COMPUTATION_API_VERSION);
    else if (name == api::EnvNames::computationName)
        return api::Object(mName);
    return api::Object();
}
 
api::Result StandaloneEnvironment::setEnvironment(const std::string& /*name*/, 
                                            api::ObjectConstRef /*value*/)
{
    return api::Result::Unknown;
}
  

api::Result  StandaloneEnvironment::initializeComputation(api::ObjectRef config)
{
    api::Result r = mComputation->configure("initialize",config);
    if (r != api::Result::Invalid) {
        mComputation->configure("start",api::Object());
    }
    return r;
}

api::Result  StandaloneEnvironment::sendMessage(const api::MessageContentConstPtr& content,
                        api::ObjectConstRef options)
{
    StandaloneMetadata::Ptr md(new StandaloneMetadata(content,options));
    api::Message msg(md,content);
    return mComputation->onMessage(msg);
}

void StandaloneEnvironment::performIdle()
{
    mComputation->onIdle();
}
 
void StandaloneEnvironment::shutdownComputation()
{
    mComputation->configure("stop",api::Object());
}

void StandaloneEnvironment::loadDso(const std::string& dsoName)
{
    typedef api::Computation* (*CREATE_FP)(api::ComputationEnvironment*);
    mDso = dlopen(dsoName.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!mDso) {
        std::string msg("Failed to load computation dso '" +
                        dsoName + "': " + safeDlerror());
        throw LoadError(msg);
    }

    CREATE_FP createFunc = (CREATE_FP)dlsym(mDso,COMPUTATION_CREATE_FUNC);
    if (!createFunc) {
        dlclose(mDso);
        mDso = nullptr;
        std::string msg("Failed to load symbol '" + 
                        std::string(COMPUTATION_CREATE_FUNC) +
                        "' from computation dso '" +
                        dsoName + "': " + safeDlerror());
        throw LoadError(msg);
    }

    mComputation = createFunc(this);
    if (!mComputation) {
        dlclose(mDso);
        mDso = nullptr;
        std::string msg("Computation creation failed in computation dso '"+
                        dsoName + "': " + safeDlerror());
        throw LoadError(msg);
    }
}

}
}

    
    
