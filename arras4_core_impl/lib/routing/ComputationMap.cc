// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ComputationMap.h"
#include <exceptions/KeyError.h>

#include <algorithm>

#if defined(JSONCPP_VERSION_MAJOR)
#define memberName name
#endif

// Note: some improvements can be made in this code if
// we switch to a newer version of jsoncpp
namespace arras4 {
    namespace impl {

ComputationMap::ComputationMap(const api::UUID& sessionId,
                               api::ObjectConstRef computationsData)
{

    for (api::ObjectConstIterator compIt = computationsData.begin();
         compIt != computationsData.end(); ++compIt) {
        api::UUID compId((*compIt)["compId"].asString());
        mCompNameToId[compIt.memberName()] = compId;   // memberName() is DEPRECATED in later jsoncpp versions,
        mCompIdToName[compId] = compIt.memberName();   // -- switch to 'name()' when possible

        api::Address addr;
        addr.computation = compId;
        addr.node = api::UUID((*compIt)["nodeId"].asString());
        addr.session = sessionId;
        mCompIdToAddress[compId] = addr;
    }

    // special entry for (client)
    mCompNameToId["(client)"] = api::UUID::null;
    mCompIdToName[api::UUID::null] = "(client)";
    api::Address clientAddr;
    clientAddr.session = sessionId;
    mCompIdToAddress[api::UUID::null] = clientAddr;
}

ComputationMap::~ComputationMap()
{
}

const api::Address& 
ComputationMap::getComputationAddress(const api::UUID& uuid) const
{
    auto it = mCompIdToAddress.find(uuid);
    if (it == mCompIdToAddress.end()) {
        std::string tmp("Computation id " + uuid.toString() + " not found");
        throw KeyError(tmp.c_str());
    }
    return it->second;
}
 
const api::UUID& 
ComputationMap::getComputationId(const std::string& name) const
{
    auto it = mCompNameToId.find(name);
    if (it == mCompNameToId.end()) {
        std::string tmp("Computation " + name + " not found");
        throw KeyError(tmp.c_str());
    }
    return it->second;    
}       

const std::string& 
ComputationMap::getComputationName(const api::UUID& uuid) const
{
    auto it = mCompIdToName.find(uuid);
    if (it == mCompIdToName.end()) {
        std::string tmp("Computation id " + uuid.toString() + " not found");
        throw KeyError(tmp.c_str());
    }
    return it->second; 
}

void 
ComputationMap::getAllAddresses(api::AddressList& out,
                                bool includeClient /* = false */) const
{
    for (const auto& entry : mCompIdToAddress) {
        if (includeClient || entry.first.valid())
            out.push_back(entry.second);
    }
}
    
} 
} 

