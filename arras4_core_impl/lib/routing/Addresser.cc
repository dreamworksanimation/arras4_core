// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Addresser.h"
#include "Addressing.h"
#include "ComputationMap.h"

#include <message_impl/Envelope.h>
#include <exceptions/InternalError.h>

#include <unordered_set>
#include <algorithm>

// Note: some improvements can be made in this code if
// we switch to a newer version of jsoncpp
namespace arras4 {
namespace impl {

Addresser::Addresser() :
    mAddressing(new Addressing)
{
}

void Addresser::update(const api::UUID& sourceCompId,
                     const ComputationMap& compMap,
                     api::ObjectConstRef aMessageFilters)
{
    Addressing* addressing = new Addressing(sourceCompId,
					    compMap,
					    aMessageFilters);
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    mAddressing.reset(addressing);
}

Addresser::~Addresser() 
{
}

void 
Addresser::address(Envelope& envelope) const
{
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    envelope.metadata()->from() = mAddressing->sourceAddress();
    envelope.to() = mAddressing->addresses(envelope.metadata()->routingName());
}

void 
Addresser::addressToAll(Envelope& envelope) const
{
    std::lock_guard<std::mutex> lock(mUpdateMutex);
    envelope.metadata()->from() =  mAddressing->sourceAddress();
    envelope.to() = mAddressing->allAddresses();
}

// address to an explicit address or list of addresses
bool
Addresser::addressTo(Envelope& envelope, 
                     api::ObjectConstRef addresses) const
{
    {
        std::lock_guard<std::mutex> lock(mUpdateMutex);
	envelope.metadata()->from() = mAddressing->sourceAddress();
    }
    api::Address addr;
    try {
        if (addresses.isArray()) {
            for (api::ObjectConstIterator it = addresses.begin();
                 it != addresses.end(); ++it) {
                addr.fromObject(*it);
                envelope.to().push_back(addr);
            }
        } else {
            addr.fromObject(addresses);
            envelope.to().push_back(addr);
        }
    } catch (std::exception&) { // our version of jsoncpp doesn't seem
        return false;           // to expose a specific exception
    }
    return true;
}

}
}

