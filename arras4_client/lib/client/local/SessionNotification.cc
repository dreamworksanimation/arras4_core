// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>
#include <boost/algorithm/string.hpp>
#include <http/http_types.h>
#include <http/HttpException.h>
#include <http/HttpResponse.h>
#include <http/HttpRequest.h>
#include <shared_impl/Platform.h>
#include <unistd.h> // for getpid()

#include "SessionNotification.h"

namespace {
    const char* DWA_CONFIG_ENV_NAME = "DWA_CONFIG_SERVICE";
    const char* ARRAS_CONFIG_PATH = "/serve/jose/arras/endpoints/";
    const char* COORDINATOR_CONFIG_ENDPOINT = "/coordinator/url";
}

namespace arras4 {
namespace client {

std::string
SessionNotification::getCoordinatorUrl()
{
    // Get dwa studio configuration service url from env
    const char* configSrvUrl = std::getenv(DWA_CONFIG_ENV_NAME);
    if (configSrvUrl == nullptr) {
        throw std::invalid_argument(std::string("Undefined environment variable: ") + DWA_CONFIG_ENV_NAME);
    }
    const char* studio = std::getenv("STUDIO");
    if (studio == nullptr) {
        throw std::invalid_argument(std::string("Undefined environment variable: ") + "STUDIO");
    }
    std::string datacenter = studio;
    boost::to_lower(datacenter);

    // Build the resource url
    std::string url(configSrvUrl);
    url += ARRAS_CONFIG_PATH;
    url += datacenter;
    url += "/";
    url += "prod";
    url += COORDINATOR_CONFIG_ENDPOINT;


    // Make the service request
    network::HttpRequest req(url);
    req.setUserAgent(mUserAgent);
    const network::HttpResponse& resp = req.submit();

    if (resp.responseCode() == network::HTTP_OK) {
        std::string ret;
        if (resp.getResponseString(ret)) {
            return ret;
        }
        throw network::HttpException("Configuration service returned empty response");
    } else if (resp.responseCode() == network::HTTP_SERVICE_UNAVAILABLE) {
        const std::string& msg = "Configuration service unavailable: " + url;
        throw network::HttpException(msg);
    } else {
        const std::string& msg = std::string("Unexpected response code from configuration service. ") +
                                 "The response code was : " +
                                 std::to_string(resp.responseCode()) + ", the url was : " + url;
        throw network::HttpException(msg);
    }
}

void
SessionNotification::updateCores(api::ObjectRef& aDefinition)
{
    //
    // Convert maxCores into cores for the computation
    //
    api::ObjectRef computations = aDefinition["computations"];
    for (auto it = computations.begin(); it != computations.end(); it++) {
        api::ObjectRef computation = *it;
        if (computation.isMember("requirements")) {
            api::ObjectRef requirements = computation["requirements"];

            if (requirements.isMember("resources")) {
                api::ObjectRef resources = requirements["resources"];

                //
                // if cores isn't specified then specify it
                //
                api::ObjectConstRef coresVal = resources["cores"];

                if (!coresVal.isNumeric() || (coresVal.asInt64() < 0)) {

                    if (resources.isMember("maxCores")) {
                        unsigned maxCores = 1024;
                        api::ObjectConstRef maxVal = resources["maxCores"];
                        if (maxVal.isNumeric() && (maxVal.asInt64() >= 0)) {
                            maxCores = maxVal.asUInt();
                        }

                        // don't allow more concurrency than there is available on the machine
                        unsigned assignedCores = std::thread::hardware_concurrency() - 1;
                        if (assignedCores > maxCores)
                            assignedCores = maxCores;

                        resources["cores"] = Json::Value(assignedCores);

                        // remove minCores/maxCores because we have set actual cores
                        resources.removeMember("maxCores");
                        resources.removeMember("minCores");
                    } else {
                        // default to one if cores aren't given
                        resources["cores"] = 1;
                    }
                }

                // only need to do one so go ahead and break
                break;
            }
        }
    }
}

void
SessionNotification::registerSession()
{
    api::Object requestObject;

    impl::PlatformInfo info;
    // is this still needed?
    requestObject["session"] = "empty";

    // put the platform information into the request
    getPlatformInfo(info);
    requestObject["node_name"] = info.mNodeName;
    requestObject["os_name"] = info.mOSName;
    requestObject["os_version"] = info.mOSVersion;
    requestObject["os_distribution"] = info.mOSDistribution;
    requestObject["brief_version"] = info.mBriefVersion;
    requestObject["brief_distribution"] = info.mBriefDistribution;

    // add in the session id
    requestObject["session_id"] = mSessionId;

    // add in the user's name and the process id
    const char* logname = std::getenv("LOGNAME");
    if (logname == nullptr) {
        return;
    }
    requestObject["username"] = logname;
    requestObject["pid"] = getpid();

    // copy over the sessionoptions
    requestObject["production"] = mSessionOptions["production"];
    requestObject["sequence"] = mSessionOptions["sequence"];
    requestObject["shot"] = mSessionOptions["shot"];
    requestObject["assetGroup"] = mSessionOptions["assetGroup"];
    requestObject["asset"] = mSessionOptions["asset"];
    requestObject["department"] = mSessionOptions["department"];
    requestObject["team"] = mSessionOptions["team"];
    requestObject["metadata"] = mSessionOptions["metadata"];

    // convert maxCores into an actual number of cores and add the definition
    api::Object definition = mDefinition;
    updateCores(definition);
    requestObject["sessionDef"] = definition;

    std::string url = getCoordinatorUrl() + "/sessions/local";

    network::HttpRequest req(url, network::POST);
    req.setUserAgent(mUserAgent);

    // this throws, let caller handle it
    std::string httpMsgBody = api::objectToString(requestObject);
    req.setContentType(arras4::network::APPLICATION_JSON);

    // Make the Session Request
    ARRAS_LOG_DEBUG_STR("POST " << url);
    ARRAS_LOG_DEBUG_STR("POST Body: " << httpMsgBody);
    const network::HttpResponse& resp = req.submit(httpMsgBody);

    // there are 4 ways an error could be indicated:
    //  - no response body
    //  - response body not JSON
    //  - response body is JSON containing an "error" member
    //  - response code is not HTTP_OK

    std::string responseString;
    bool emptyResponse = false;
    std::string responseMessage;
    api::Object responseObject;

    if (resp.getResponseString(responseString)) {
        responseMessage = responseString;
        ARRAS_LOG_DEBUG_STR("Http Response (" << resp.responseCode() << "): " << responseMessage);
        // got a string : is it JSON?
        try {
            api::stringToObject(responseString, responseObject);

            // its an object, does it have a message?
            if (responseObject.isMember("message")) {
                if (responseObject["message"].isString()) {
                    responseMessage = responseObject["message"].asString();
                }
            }
        } catch (api::ObjectFormatError&) {
            // not an object : assume its an error message string
        }
    } else {
        // no response body
        emptyResponse = true;
    }

    if (!emptyResponse && resp.responseCode() == network::HTTP_OK) {

    } else if (resp.responseCode() == network::HTTP_SERVICE_UNAVAILABLE) {
        // don't care about error message : status code is enough to tell us
        // what happened...
        std::string err("Insufficient resources available to fill this request");
        if (!responseMessage.empty())  {
            err += ": " + responseMessage;
        }
    } else {
        // unknown status code must be an error

        std::string err = std::string("Server responded with error code ") + std::to_string(resp.responseCode());
        if (!responseMessage.empty())
            err += ", message: " + responseMessage;
    }

}

void SessionNotification::registerThread() {
    try {
        // register with coordinator
        registerSession();
        // this object self destructs when the registration is complete
        delete this;
    } catch (...) {
        // any exceptions should just be ignored since this is just notification 
    }
}

SessionNotification::SessionNotification(
        api::ObjectConstRef aDefinition,
        api::ObjectConstRef aSessionOptions,
        const std::string& aSessionId,
        const std::string& aUserAgent) :
        mDefinition(aDefinition),
        mSessionOptions(aSessionOptions),
        mSessionId(aSessionId),
        mUserAgent(aUserAgent) {

    mThread = std::thread(&SessionNotification::registerThread, this);
    mThread.detach();
}

} // end namespace client
} // end namespace arras4

