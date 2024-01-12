// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Address.h"

namespace arras4 {
    namespace api {

const Address& Address::null = Address();
      
std::ostream &operator<<(std::ostream &s, const Address &addr)
{
	return s << "Session: " << addr.session 
             << " Node: " << addr.node
             << " Comp: " << addr.computation;
}  

void Address::toObject(ObjectRef obj) const
{
    obj["session"] = session.toString();
    obj["node"] = node.toString();
    obj["computation"] = computation.toString();
}

void Address::fromObject(ObjectConstRef obj)
{
    session = obj["session"].asString();
    node = obj["node"].asString();
    computation = obj["computation"].asString();
}

}
}
