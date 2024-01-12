// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_COMPUTATIONMAP_H__
#define __ARRAS4_COMPUTATIONMAP_H__

#include <message_api/messageapi_types.h>
#include <message_api/Object.h>
#include <message_api/Address.h>

#include <memory>
#include <map>
#include <string>


/** ComputationMap stores the mapping of computation names / ids to addresses for
 * a particular session.
 * - Given a computation name, you can fetch id or the full address of the computation.
 * - Given a computation id, you can fetch the full address of the computation or the
 *   computation name
 *
 * The session client has name "(client)" and computation id UUID::null
 *
 * The map is initialized from routing data provided by coordinator to the node.
 * A mutex is not required, since the data is read-only after construction
 *
 * Failed lookups in any of the get functions generate a KeyError exception.
 **/
namespace arras4 {
    namespace impl {
        
class ComputationMap 
{            
public:
    // computationsData is the data under the 
    // "routing"/<sessionId>/"computations" key, 
    // sent from coordinator to node.
    ComputationMap(const api::UUID& sessionId,
                   api::ObjectConstRef computationsData);
    ~ComputationMap();

    // throws KeyError if request cannot be found
    const api::Address& getComputationAddress(const std::string& name) const
        { return getComputationAddress(getComputationId(name)); }
    const api::Address& getComputationAddress(const api::UUID& uuid) const;
    const api::UUID& getComputationId(const std::string& name) const;
    const std::string& getComputationName(const api::UUID& uuid) const;
    
    void getAllAddresses(api::AddressList& out, bool includeClient=false) const;
    
private:            
    std::map<std::string, api::UUID> mCompNameToId;
    std::map<api::UUID, std::string> mCompIdToName;
    std::map<api::UUID, api::Address> mCompIdToAddress;
};

}
} 

#endif 

