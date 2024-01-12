// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CONTENTREGISTRYH__
#define __ARRAS4_CONTENTREGISTRYH__

#include "messageapi_types.h"
#include "UUID.h"

#include <string>
#include <memory>
#include <mutex>
#include <map>

// ContentRegistry allows Arras to deserialize message content into
// ObjectContent instances. The registry can create a new ObjectContent 
// instance, given the class id from the message. 
// ObjectContent subclasses are usually registered automatically during 
// static initialization, via a mechanism defined in ContentMacros.h

namespace arras4 {
    namespace api {

class ContentFactory
{
public:
    virtual ObjectContent* create(unsigned version) const = 0;
};

template<typename T>
class SimpleContentFactory : public ContentFactory
{
public:
    T* create(unsigned /*version*/) const { return new T; }
};

class ContentRegistry 
{
public:
    
    static ContentRegistry* singleton();
    void registerFactory(const ClassID& classId,const ContentFactory* cls);
    ObjectContent* create(const ClassID& classId, unsigned version) const;
 
private:
    mutable std::mutex mMutex;
    std::map<ClassID,const ContentFactory*> mMap;
};

    }
}
#endif

