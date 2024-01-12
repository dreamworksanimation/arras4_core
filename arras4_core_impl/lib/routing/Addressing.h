// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_ADDRESSING_H__
#define __ARRAS4_ADDRESSING_H__

#include <message_api/messageapi_types.h>
#include <message_api/Object.h>
#include <message_api/Address.h>


#include <map>
#include <memory>
#include <string>

/** Addressing stores a computation's message filters in an efficient format
 * that can be used by Addresser to address messages
 * 
 * Each Addressing instance is created to handle messages coming from one particular
 * computation. The data is initialized using the message filters sent to node
 * by the coordinator : the full set of these specifies the types of messages
 * that may be sent between a pair of computations (source,dest). Since the
 * filters are described in terms of computation names, Addressing initialization
 * needs a ComputationMap object to map between computation names and addresses.
 *
 * To generate addressing for messages coming from the client, specify a
 * sourceCompId of UUID::null
 *
 * Addressing also handles a special case where messages should be sent to
 * all computations regardless of filters : this applies, for example, to
 * Ping messages
 *
 * A mutex is not required, since the data is read-only after construction
 **/
namespace arras4 {
    namespace impl {
        
class ComputationMap;
class Envelope;
            
class Addressing {

public:
            
    // create no-op Addressing that doesn't map any messages
    Addressing() {}
    ~Addressing();

    // create Addressing using session routing data
    // aRoutingData is the data under the "routing"/"messageFilters" 
    // key, sent from coordinator to node.
    Addressing(const api::UUID& sourceCompId,
                const ComputationMap& compMap,
                api::ObjectConstRef aMessageFilters);

    const api::Address sourceAddress() const { return mSourceAddress; }

    // get list of addresses for a given message based on message filters
    const api::AddressList& addresses(const std::string& routingName) const;
    // get all computation addresses except client
    const api::AddressList& allAddresses() const;

private:
            
    api::Address mSourceAddress;

    // map each routing name to the destination addresses it
    // should be sent to
    typedef std::map<std::string,api::AddressList> MessageAddressMap;
    MessageAddressMap mMessageAddressMap;

    // destinations for messages not listed in the message address map
    api::AddressList mDefaultAddresses;
              
    // all destinations, excluding client
    api::AddressList mAllAddresses;
};

}
}


#endif

