// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGECONTENTH__
#define __ARRAS4_MESSAGECONTENTH__

#include "messageapi_types.h"
#include <memory>

namespace arras4 {
    namespace api {

// MessageContent objects provide an interface to the 'content' part
// of a message. Different subclasses may be used in different contexts.
// For example, internally Arras often uses the subclass 'OpaqueContent',
// which provides direct access to the content data as a serialized block 
// but doesn't have any type-specific methods.
//
// Generally, user code will interact with a subclass of ObjectContent,
// which implements the serialize/deserialize interface together with
// methods specific to the type of content.
//
// classId gives a 'type' to the content which is independent of the
// particular subclass used to represent it. The format() function 
// provides a hint to the interface of a particular object, but 
// generally C++ code will attempt a dynamic_cast<> to a specific subclass.
//
// classVersion provides a way to version iterations of a particular class.
// It is preserved by message transmission but not otherwise used by Arras.
// More elaborate version mechanisms can be used inside the content data
// if desired.
//
// 'defaultRoutingName' is the routing name to be used with a message 
// containing this content, in cases where an explicit routing hasn't
// been specified : usually it is the 'name' of the class.
class MessageContent
{
public:
    enum Format {
        Object,
        Opaque
    };
    virtual ~MessageContent() {}

    virtual const ClassID& classId() const = 0;
    virtual unsigned classVersion() const = 0;
    virtual const std::string& defaultRoutingName() const = 0;
    virtual Format format() const = 0;
  
};
        
}
}
#endif
