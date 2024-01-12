// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_OBJECT_CONTENTH__
#define __ARRAS4_OBJECT_CONTENTH__

#include "MessageContent.h"

namespace arras4 {
    namespace api {

// This subclass of MessageContent provides a serializable, C++ class-based
// interface to message content. Subclasses of ObjectContent are typically
// implemented in user code using the macros in 'ContentMacros.h'.
//
// Generally, on receiving a message, user code will attempt to
// dynamically cast the content to an appropriate subclass of ObjectContent
// based on its class id. If the cast fails, this indicates that the desired 
// subclass was not registered with the Arras system at deserialization time.
class ObjectContent : public MessageContent 
{
public:

    virtual void serialize(api::DataOutStream& to) const = 0;
    virtual void deserialize(api::DataInStream& from, unsigned version) = 0;

    // this function is deprecated and only for use in legacy message chunking
    // support
    virtual size_t serializedLength() const { return 0;}
};
        
}
}
#endif
