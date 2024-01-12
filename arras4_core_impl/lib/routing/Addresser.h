// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_ADDRESSER_H__
#define __ARRAS4_ADDRESSER_H__

#include <message_api/messageapi_types.h>
#include <message_api/Object.h>
#include <message_api/Address.h>


#include <map>
#include <memory>
#include <string>
#include <mutex>

/** Addresser fills in the 'to' addresses for a message coming from a 
 * particular computation. This task is performed either by the executor process 
 * hosting the source computation, or by node (in the case of messages coming 
 * from the client).
 * 
 * Each Addresser instance handles messages coming from one particular
 * computation. The data is initialized using the message filters sent to node
 * by the coordinator : this is converted to an optimized form and stored by the
 * 'Addressing' class.
 *
 * To generate an addresser for messages coming from the client, specify a
 * sourceCompId of UUID::null
 *
 * Addresser also handles a special case where messages should be sent to
 * all computations regardless of filters : this applies, for example, to
 * Ping messages
 *
 * Addressers are created with no initial addressing. The update() function
 * can be called at any time to set/reset the addressing based on a set
 * of message filters. update() is a threadsafe function.
 **/
namespace arras4 {
    namespace impl {

class ComputationMap;
class Envelope;
class Addressing;

class Addresser {

public:
            
    // use update() to initialize the Addresser after construction
    Addresser();

    ~Addresser();

    // initialize/update the address using session routing data
    // aRoutingData is the data under the "routing"/"messageFilters" 
    // key, sent from coordinator to node.
    void update(const api::UUID& sourceCompId,
                const ComputationMap& compMap,
                api::ObjectConstRef aMessageFilters);

    // address to appropriate computations based on message filters
    void address(Envelope& envelope) const;

    // address to all computations in session (except client), 
    // ignoring filters
    void addressToAll(Envelope& envelope) const;

    // address to an explicit address or list of addresses. returns
    // false if addresses was invalid
    bool addressTo(Envelope& envelope, 
                   api::ObjectConstRef addresses) const;

private:
     
    mutable std::mutex mUpdateMutex;
    std::unique_ptr<Addressing> mAddressing;
};

}
}


#endif

