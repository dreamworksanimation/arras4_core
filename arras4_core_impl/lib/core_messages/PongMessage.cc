// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "PongMessage.h"
#include <sstream>
#include <iomanip>

namespace arras4 {
    namespace impl {

ARRAS_CONTENT_IMPL(PongMessage);

const std::string PongMessage::RESPONSE_ACKNOWLEDGE("acknowledge");
const std::string PongMessage::RESPONSE_STATUS("status");


void PongMessage::serialize(api::DataOutStream& to) const
{
    to << mSourceId << mName << mEntityType << mResponseType
       << mTimeSecs << mTimeMicroSecs << mPayload;
}

void PongMessage::deserialize(api::DataInStream& from, 
                                 unsigned /*version*/)
{
    from >> mSourceId >> mName >> mEntityType >> mResponseType
         >> mTimeSecs >> mTimeMicroSecs >> mPayload;
}

bool PongMessage::payload(api::ObjectRef aJson) const
{
    try {
        api::stringToObject(mPayload,aJson);
    } catch (api::ObjectFormatError&) {
        return false;
    }
    return true;
}

bool 
PongMessage::setPayload(api::ObjectConstRef aJson)
{
    mPayload = api::objectToString(aJson);
    return true;
}

std::string 
PongMessage::timeString() const
{
    tm date;
    time_t secs = static_cast<time_t>(mTimeSecs);
    localtime_r(&secs, &date);
    std::stringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << date.tm_mday << "/"
       << std::setw(2) << date.tm_mon << "/"
       << (date.tm_year + 1900) << " "
       << std::setw(2) << date.tm_hour << ":"
       << std::setw(2) << date.tm_min << ":"
       << std::setw(2) << date.tm_sec << ","
       << std::setw(3) << (mTimeMicroSecs / 1000);
    return ss.str();
}

}
}
 
