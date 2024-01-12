// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Addressing.h"
#include "ComputationMap.h"

#include <message_impl/Envelope.h>
#include <exceptions/InternalError.h>
#include <exceptions/KeyError.h>

#include <unordered_set>
#include <algorithm>

#if defined(JSONCPP_VERSION_MAJOR)
#define memberName name
#endif

// Note: some improvements can be made in this code if
// we switch to a newer version of jsoncpp
namespace arras4 {
namespace impl {

Addressing::Addressing(const api::UUID& sourceCompId,
                       const ComputationMap& compMap,
                       api::ObjectConstRef aMessageFilters)
{
    // The construction of this object is relatively complex,
    // in order to get the best performance in use. All messages
    // that are explicitly mentioned in one of the filters for this
    // source computation will get an entry in mMessageAddressMap, listing
    // all destination addresses for that message type. Messages
    // that aren't explicitly listed go to all addresses in mDefaultAddresses, 
    // which contains all computations that don't have an explicit "accept" list.
    //

    compMap.getAllAddresses(mAllAddresses);

    try {
	mSourceAddress = compMap.getComputationAddress(sourceCompId);
    } catch (KeyError&) {
	// if source computation has no map entry, then Addressing
	// is left empty
	return;
    }

    const std::string sourceName(compMap.getComputationName(sourceCompId));
    api::ObjectConstRef filters = aMessageFilters[sourceName];

    // process each filter, adding an entry to mMessageAddressMap for each
    // routing name mentioned as a destination. mDefaultAddresses keeps
    // the addresses of the computations that should receive all unmapped 
    // messages. When a new message map entry is added, it is initialized from
    // this list.
    for (api::ObjectConstIterator destIt = filters.begin();
         destIt != filters.end(); ++destIt) {
        std::string destName = destIt.memberName(); // memberName() is DEPRECATED in later jsoncpp versions,
                                                    // -- switch to 'name()' when possible
        const api::Address& destAddr = compMap.getComputationAddress(destName);
        std::unordered_set<std::string> ignoreSet;
        
        // process accept list, if there is one
        api::ObjectConstRef accepts = (*destIt)["accept"];
        bool foundAnAccept = false;

        // for each entry in the accepts list, create a message map entry if necessary,
        // and add this computation to the entry
        if (accepts.isArray()) {
            for (api::ObjectConstIterator acceptIt = accepts.begin();
                 acceptIt != accepts.end(); ++acceptIt) {
                if ((*acceptIt).isString()) {
                        const std::string& msg = (*acceptIt).asString();
                        auto inserted = mMessageAddressMap.insert(MessageAddressMap::value_type(msg,mDefaultAddresses));
                        inserted.first->second.push_back(destAddr);
                        foundAnAccept = true;
                }
            }
        }
   

        // if there were any "accept" entries, the "ignore" entries are ignored and we are done
        if (foundAnAccept) {                
            continue;
        }
        
        // collect all the "ignore" message names in a set for fast lookup
        api::ObjectConstRef ignores = (*destIt)["ignore"];
        if (ignores.isArray()) {
            for (api::ObjectConstIterator ignoreIt = ignores.begin();
                 ignoreIt != ignores.end(); ++ignoreIt) {
                if ((*ignoreIt).isString()) {
                    ignoreSet.insert((*ignoreIt).asString());
                }
            }
        }

        // Now the computation should receive all messages except those in ignoreSet.
        // We also fall through to here if there were no subobjects  in the filter
        // section for this destination computation (although there might have been just the string "*" 
        // : this isn't a subobject). In this case ignoreSet is empty.

        // add this destination to already mapped messages not in the ignore set. Erase entries
        // in ignoreSet as we go, so that we are left with just the unmapped messages that this 
        // computation ignores
        for (auto it = mMessageAddressMap.begin();
             it != mMessageAddressMap.end(); ++it) {
            auto is_it = ignoreSet.find(it->first);
            if (is_it == ignoreSet.end()) {
                it->second.push_back(destAddr);
            } else {
                ignoreSet.erase(is_it);
            }
        }
        
        // ignoreSet now contains all messages that we should ignore
        // that don't yet have an entry in the message address map. We
        // need to create an map entry, since these messages no longer go to 
        // all default computations (because they don't go to this one)
        for (auto is_it = ignoreSet.begin(); 
             is_it != ignoreSet.end(); ++is_it) {
            mMessageAddressMap.insert(MessageAddressMap::value_type(*is_it,mDefaultAddresses));
        }

        // everything in the ignore set has been processed, so this computation should
        // receive all remaining unlisted messages.
        mDefaultAddresses.push_back(destAddr);
    }
}

Addressing::~Addressing() 
{
}

const api::AddressList&
Addressing::addresses(const std::string& routingName) const
{
    auto it = mMessageAddressMap.find(routingName);
    if (it == mMessageAddressMap.end()) 
        return mDefaultAddresses;
    else
        return it->second;
}

const api::AddressList&
Addressing::allAddresses() const
{
    return mAllAddresses;
}

}
}

