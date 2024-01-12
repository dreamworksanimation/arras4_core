// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "AcapAPI.h"

#include <sstream>

namespace {
    const char* INVALID_SESSION_ID = "invalid";
} // namespace

namespace arras4 {
namespace client {

AcapRequest::AcapRequest(const std::string& aService,
                         const std::string& aUserAgent,
                         const std::string& aPamSite,
                         const std::string& aPamProject)
   : mService(aService)
   , mUserAgent(aUserAgent)
   , mPamSite(aPamSite)
   , mPamProject(aPamProject)
{
}

AcapRequest::~AcapRequest()
{
}

std::string
AcapRequest::uri() const
{
    std::ostringstream oss;
    oss << "service=" << mService << "&";
    oss << "pam_site=" << mPamSite << "&";
    oss << "pam_project=" << mPamProject;
    return oss.str();
}

AcapResponse::AcapResponse()
    : mSessionID(INVALID_SESSION_ID)
    , mErrorCode(OTHER_ERROR)

{
}


AcapResponse::AcapResponse(const std::string& error, ErrorCode errorCode)
    : mSessionID(INVALID_SESSION_ID)
    , mErrorCode(errorCode)
    , mError(error)
{
}

AcapResponse::AcapResponse(const SessionList& sessions, const SessionID& sessionID)
    : mSessions(sessions)
    , mSessionID(sessionID)
    , mErrorCode(NO_ERROR_ACAP)
    , mError("")
{
}


AcapResponse::~AcapResponse()
{
}


SessionOptions::SessionOptions(const SessionOptions& obj)
{
    setProduction(obj.getProduction());
    setSequence(obj.getSequence());
    setShot(obj.getShot());
    setAssetGroup(obj.getAssetGroup());
    setAsset(obj.getAsset());
    setDepartment(obj.getDepartment());
    setTeam(obj.getTeam());
    setId(obj.getId());
    setMetadata(obj.getMetadata());
}


SessionOptions&
SessionOptions::setProduction(const std::string& aProduction)
{
    mProduction = aProduction;
    return *this;
}


SessionOptions&
SessionOptions::setSequence(const std::string& aSequence)
{
    mSequence = aSequence;
    return *this;
}


SessionOptions&
SessionOptions::setShot(const std::string& aShot)
{
    mShot = aShot;
    return *this;
}



SessionOptions&
SessionOptions::setAssetGroup(const std::string& aAssetGroup)
{
    mAssetGroup = aAssetGroup;
    return *this;
}


SessionOptions&
SessionOptions::setAsset(const std::string& aAsset)
{
    mAsset = aAsset;
    return *this;
}



SessionOptions&
SessionOptions::setDepartment(const std::string& aDepartment)
{
    mDepartment = aDepartment;
    return *this;
}


SessionOptions&
SessionOptions::setTeam(const std::string& aTeam)
{
    mTeam = aTeam;
    return *this;
}


SessionOptions&
SessionOptions::setId(const std::string& anId)
{
    mId = anId;
    return *this;
}


SessionOptions&
SessionOptions::setMetadata(api::ObjectConstRef aMetadata)
{
    mMetadata = aMetadata;
    return *this;
}


SessionOptions&
SessionOptions::setMetadata(const std::string& aMetadataJSON)
{
    api::Object metadata;
    jsonStringToObj(aMetadataJSON, metadata);
    return setMetadata(metadata);
}


void
SessionOptions::jsonStringToObj(const std::string& aJSON,api::ObjectRef obj) const
{
    if (!aJSON.empty()) {
        api::stringToObject(aJSON,obj);
    }
}

namespace {
void appendWithSpace(std::string& s,
		     const std::string& a)
{
    if (!a.empty()) {
	if (!s.empty()) s += " ";
	s += a;
    }
}

void
addString(api::ObjectRef aObject, const std::string& aName, const std::string& aValue)
{
    /* We are going to treat empty strings as null values */
    if (!aValue.empty()) {
        aObject[aName] = aValue;
    } else {
        aObject[aName] = api::Object();
    }
}

} // end anonymous namespace


std::string 
SessionOptions::getTitle() const
{
    std::string title(mSequence);
    appendWithSpace(title,mShot);
    appendWithSpace(title,mAssetGroup);
    appendWithSpace(title,mAsset);
    if (title.empty()) {
	title = "[No Title]";
    }
    return title;
}

void
SessionOptions::appendToObject(api::ObjectRef aObject) const
{
    addString(aObject, "production", getProduction());
    addString(aObject, "sequence", getSequence());
    addString(aObject, "shot", getShot());

    addString(aObject, "assetGroup", getAssetGroup());
    addString(aObject, "asset", getAsset());

    addString(aObject, "department", getDepartment());
    addString(aObject, "team", getTeam());
    addString(aObject, "id", getId());  // client defined key/id string.

    aObject["metadata"] = getMetadata();
}

} // namespace client
} // namespae arras

