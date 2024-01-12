// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ContentRegistry.h"

namespace arras4 {
    namespace api {
  
/*static*/ ContentRegistry* ContentRegistry::singleton()
{
    static ContentRegistry s;
    return &s;
}
    
void ContentRegistry::registerFactory(const ClassID& classId,const ContentFactory* factory)
{
    if (factory) {
        std::lock_guard<std::mutex> lock(mMutex);
        mMap.insert(std::map<ClassID,const ContentFactory*>::value_type(classId,factory));
    }
}

ObjectContent* ContentRegistry::create(const ClassID& classId, unsigned version) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mMap.find(classId);
    if (it == mMap.end())
        return nullptr;
    else
        return it->second->create(version);
}

}
}
