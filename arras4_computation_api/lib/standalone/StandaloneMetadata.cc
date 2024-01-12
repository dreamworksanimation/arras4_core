// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "StandaloneMetadata.h"

#include <message_api/messageapi_names.h>


namespace arras4 {
    namespace impl {

StandaloneMetadata::StandaloneMetadata()
{
    mInstanceId.regenerate();
    mSourceId.regenerate();
    mCreationTime = api::ArrasTime::now();
}

// create default metadata for the given content
StandaloneMetadata::StandaloneMetadata(const api::MessageContentConstPtr& content,
                                       api::ObjectConstRef options)
    : StandaloneMetadata()
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

api::Object StandaloneMetadata::get(const std::string& optionName) const
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
    
void  StandaloneMetadata::toObject(api::ObjectRef obj)
{
    obj["instanceId"] = mInstanceId.toString(); 
    obj["sourceId"] = mSourceId.toString();
    obj["creationTime"]["seconds"] = mCreationTime.seconds;
    obj["creationTime"]["microseconds"] =  mCreationTime.microseconds;
    mFrom.toObject(obj["from"]);
    obj["routingName"] = mRoutingName;
}

void StandaloneMetadata::fromObject(api::ObjectConstRef obj)
{
    mInstanceId = obj["instanceId"].asString();
    mSourceId = obj["sourceId"].asString();
    mCreationTime.seconds =  obj["creationTime"]["seconds"].asInt();
    mCreationTime.microseconds =  obj["creationTime"]["seconds"].asInt();
    mFrom.fromObject(obj["from"]);
    mRoutingName = obj["routingName"].asString();
}

}
}
