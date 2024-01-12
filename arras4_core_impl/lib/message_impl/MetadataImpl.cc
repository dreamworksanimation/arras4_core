// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MetadataImpl.h"
#include "messaging_version.h"

#include <message_api/messageapi_names.h>
#include <message_api/MessageFormatError.h>
#include <message_api/DataInStream.h>
#include <message_api/DataOutStream.h>
#include <message_api/ContentRegistry.h>

#include <sstream>

namespace {
    // bit masks for flags header item
    uint16_t FLAGS_MASK_TRACE = 0x01;
}

namespace arras4 {
    namespace impl {

MetadataImpl::MetadataImpl()
{
    mInstanceId.regenerate();
    mSourceId.regenerate();
    mCreationTime = api::ArrasTime::now();
    mTrace = false;
}

// create default metadata for the given content
MetadataImpl::MetadataImpl(const api::MessageContentConstPtr& content,
                           api::ObjectConstRef options)
    : MetadataImpl()
{
    if (content)
        mRoutingName = content->defaultRoutingName();
    if (options[api::MessageOptions::sourceId].isString()) {
        mSourceId = api::UUID(options[api::MessageOptions::sourceId].asString());
    }
    if (options[api::MessageOptions::routingName].isString()) {
        mRoutingName = options[api::MessageOptions::routingName].asString();
    }
}

api::Object MetadataImpl::get(const std::string& optionName) const
{
    if (optionName == api::MessageData::instanceId) 
        return mInstanceId.toString();
    if (optionName == api::MessageData::sourceId) 
        return mSourceId.toString();
    if (optionName == api::MessageData::from) {
        api::Object ret;
        mFrom.toObject(ret);
        return ret;
    }
    if (optionName == api::MessageData::routingName) 
        return mRoutingName;
    if (optionName == api::MessageData::creationTimeSecs) 
        return mCreationTime.seconds;
    if (optionName == api::MessageData::creationTimeMicroSecs) 
        return mCreationTime.microseconds;
    if (optionName == api::MessageData::creationTimeString) 
        return mCreationTime.dateTimeStr(); 
    return api::Object();
}
    
void  MetadataImpl::toObject(api::ObjectRef obj)
{
    obj["instanceId"] = mInstanceId.toString(); 
    obj["sourceId"] = mSourceId.toString();
    obj["creationTime"]["seconds"] = mCreationTime.seconds;
    obj["creationTime"]["microseconds"] =  mCreationTime.microseconds;
    mFrom.toObject(obj["from"]);
    obj["routingName"] = mRoutingName;
}

void MetadataImpl::fromObject(api::ObjectConstRef obj)
{
    mInstanceId = obj["instanceId"].asString();
    mSourceId = obj["sourceId"].asString();
    mCreationTime.seconds =  obj["creationTime"]["seconds"].asInt();
    mCreationTime.microseconds =  obj["creationTime"]["seconds"].asInt();
    mFrom.fromObject(obj["from"]);
    mRoutingName = obj["routingName"].asString();
}


// Current message format (4.0)
//
// headerLength           uint32             offset to addressCount
// messageProtocolVersion uint16             0x0302
// flags                  uint16             (was unused field "payloadType")
//     b0: trace          bit                if set, output tracing information
// instanceId             UUID (16 bytes)    unique message id
// sourceId               16 bytes           for application use
// routingName            uint16,uint8*      id for routing
// ... expansion ..       empty              new fields will go here
// addressCount           uint32             number of addresses following
// fromAddress            3*UUID             address of sender
// toAddresses            n*3*UUID           addresses of recipients
// payloadClass           UUID               message class id
// payloadClassVersion    uint32             class-defined
// payload                ...
//
// headerLength field is 42+len(routingName)    
// full header size is 66 + len(routingName) + 48*addressCount

void MetadataImpl::serialize(api::DataOutStream& to,
                             const api::AddressList& destinations) const
{
    uint16_t nameLen = static_cast<uint16_t>(mRoutingName.length());
    if (nameLen == 0)
        throw api::MessageFormatError("Routing name has zero length");

    // offset to addresses = headerLength(4) + version(2) + type(2) + 
    // instanceId(16) + originId(16) + namecount(2) + namelen 
    // = 42 + namelen
    uint32_t addrOffset = 42 + nameLen; 
    uint16_t protocolVer = (ARRAS_MESSAGING_API_VERSION_MAJOR << 8) + 
        ARRAS_MESSAGING_API_VERSION_MINOR;
    uint16_t flags = mTrace ? FLAGS_MASK_TRACE : 0;

    to << addrOffset << protocolVer << flags;
    to << mInstanceId << mSourceId << nameLen;
    if (nameLen > 0)
        to.write(mRoutingName.c_str(),nameLen);

    uint32_t nAddr = uint32_t(destinations.size() + 1);
    to << nAddr << mFrom;
    for (auto& t : destinations)
        to << t;
}

void MetadataImpl::deserialize(api::DataInStream& from,
                               api::AddressList& destinations)
{
    size_t initBytes = from.bytesRead();
    uint32_t addrOffset; 
    uint16_t protocolVer;
    uint16_t flags;

    from >> addrOffset >> protocolVer >> flags;

    mTrace = (flags & FLAGS_MASK_TRACE) != 0;

    uint16_t verMajor = static_cast<uint16_t>(protocolVer >> 8);
    // we will accept any version 4.x protocol
    if (verMajor != ARRAS_MESSAGING_API_VERSION_MAJOR) {
        std::stringstream ss;
        ss << "Incorrect messaging protocol major version " << verMajor 
           << " : expected " << ARRAS_MESSAGING_API_VERSION_MAJOR;
        throw api::MessageFormatError(ss.str());
    }
   
    uint16_t nameLen;
    from >> mInstanceId >> mSourceId >> nameLen;
    if (nameLen > 0) {
        mRoutingName.resize(nameLen);
        from.read(&mRoutingName.at(0),nameLen);
    } else {
        mRoutingName.clear();
    }

    // skip the 'extension' section, up to addrOffset
    size_t bytesRead = from.bytesRead() - initBytes;
    if (bytesRead > addrOffset) 
        throw api::MessageFormatError("Header length is too short");
    else if (bytesRead < addrOffset) {
        from.skip(addrOffset - bytesRead);
    }

    uint32_t nAddr;
    from >> nAddr;

    if (nAddr == 0)
        throw api::MessageFormatError("Message must contain 'from' address");

    from >> mFrom;

    for (unsigned i = 1; i < nAddr; ++i) {
        destinations.push_back(api::Address());
        from >> destinations.back();
    }
}

}
}
