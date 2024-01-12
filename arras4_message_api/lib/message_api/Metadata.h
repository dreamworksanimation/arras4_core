// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_METADATAH__
#define __ARRAS4_METADATAH__

#include "messageapi_types.h"
#include "UUID.h"
#include "Address.h"
#include "ArrasTime.h"
#include "Object.h"

namespace arras4 {
    namespace api {

class Metadata
{
public:
    virtual ~Metadata() {}
    virtual Object get(const std::string& optionName) const=0;
    virtual std::string describe() const=0;
    virtual void toObject(ObjectRef obj)=0;
    virtual void fromObject(ObjectConstRef obj)=0;

protected:
    Metadata() {}
    Metadata(const Metadata&)=delete;
    Metadata(Metadata&&)=delete;
    Metadata& operator=(const Metadata&)=delete;
    Metadata& operator=(Metadata&&)=delete;
};

}
}
#endif
