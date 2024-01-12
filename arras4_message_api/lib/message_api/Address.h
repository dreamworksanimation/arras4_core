// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_ADDRESSH__
#define __ARRAS4_ADDRESSH__

#include "UUID.h"
#include "Object.h"

namespace arras4 {
    namespace api {

// Address records the source and destination(s) of a message
class Address {
public:
    UUID session;
    UUID node;
    UUID computation;

    static const Address& null;
    Address() {}
    Address(const UUID& s, const UUID& n, const UUID& c) :
        session(s), node(n), computation(c) {}   
    bool isNull() const {
        return session.isNull() && node.isNull() && computation.isNull(); }

    void toObject(ObjectRef obj) const;
    void fromObject(ObjectConstRef obj);

};
   
std::ostream &operator<<(std::ostream &s, const Address &addr);

}
}
#endif
