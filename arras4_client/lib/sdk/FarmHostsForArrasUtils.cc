// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "FarmHostsForArrasUtils.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>
#include <http/HttpResponse.h>

#include <exception>
#include <sstream>

namespace {

const char* DWA_CONFIG_ENV_NAME = "DWA_CONFIG_SERVICE";
const char* SILO_PATH = "/config_sets/studio/silo/url";
const char* SILO_PROCESSES = "/processes";
const char* USER_AGENT = "farmHostsForArrasUtils";
const char* DETAIL_PATH = "/groups_detail";
const char* CANCEL_PATH = "/group/";
const char* ENV_PATH = "/env/";
const char* RUN_LIMIT_PATH = "/runlimit/";
int BAD_GROUP_ID = 0;

}

namespace arras4 {
namespace sdk {

const std::string FarmHostsForArrasUtils::STATUS_KEY = "status";
const std::string FarmHostsForArrasUtils::NUM_KEY = "num";
const std::string FarmHostsForArrasUtils::PEND_STATUS = "PEND";
const std::string FarmHostsForArrasUtils::WAIT_STATUS = "WAIT";
const std::string FarmHostsForArrasUtils::RUN_STATUS = "RUN";
const std::string FarmHostsForArrasUtils::GROUP_ID_KEY = "groupId";
const std::string FarmHostsForArrasUtils::PRODUCTION_KEY = "production";
const std::string FarmHostsForArrasUtils::USER_KEY = "user";
const std::string FarmHostsForArrasUtils::NUM_HOSTS_KEY = "num_hosts";
const std::string FarmHostsForArrasUtils::MIN_CORES_KEY = "min_cores";
const std::string FarmHostsForArrasUtils::MAX_CORES_KEY = "max_cores";
const std::string FarmHostsForArrasUtils::SHARE_KEY = "share";
const std::string FarmHostsForArrasUtils::STEERING_KEY = "steering";
const std::string FarmHostsForArrasUtils::PRIORITY_KEY = "priority";
const std::string FarmHostsForArrasUtils::MINUTES_KEY = "minutes";  // for runlimit in minutes
const std::string FarmHostsForArrasUtils::MEM_KEY = "memory";
const std::string FarmHostsForArrasUtils::SUBMISSION_LABEL_KEY = "submission_label";


FarmHostsForArrasUtils::FarmHostsForArrasUtils(const std::string& datacenter,
                                               const std::string& environment)
  : mBaseUrl(getBaseUrl(datacenter))
{
    setEnv(environment);
}

api::Object
FarmHostsForArrasUtils::getDetail() const
{
    std::string msg;
    std::string url(mBaseUrl);
    url += DETAIL_PATH;
    try {
        network::HttpRequest req(url);
        req.setUserAgent(USER_AGENT);
        const network::HttpResponse& resp = req.submit();
        const auto& responseCode = resp.responseCode();
        if (responseCode == network::HTTP_OK) {
            return getObjectFromResponse(resp);
        } else {
            msg = "unexpected response code: " + std::to_string(responseCode);
        }
    } catch (const std::exception& e) {
        msg = std::string(e.what()) + "; url: " + url;
        ARRAS_ERROR(log::Id("getFarmHostDetail") << msg);
    }
    api::Object err;
    err["error"] = msg;
    return err;
}
    
int
FarmHostsForArrasUtils::postRequest(
    const std::string& production,
    const std::string& user,
    int num_hosts,
    int min_cores_per_hosts,
    const std::string& max_cores_per_hosts,
    const std::string& share,
    const std::string& steering,
    int priority,
    int minutes,
    const std::string& mem,
    const std::string& submission_label) const
{
    std::string url(mBaseUrl);
    try {
        network::HttpRequest req(url, network::POST);
        req.setUserAgent(USER_AGENT);
        api::Object requestObj;
        requestObj[PRODUCTION_KEY] = production;
        requestObj[USER_KEY] = user;
        requestObj[NUM_HOSTS_KEY] = num_hosts;
        requestObj[MIN_CORES_KEY] = min_cores_per_hosts;
        requestObj[MAX_CORES_KEY] = max_cores_per_hosts;
        requestObj[SHARE_KEY] = share;
        requestObj[STEERING_KEY] = steering;
        requestObj[PRIORITY_KEY] = priority;
        requestObj[MINUTES_KEY] = minutes;
        requestObj[MEM_KEY] = mem;
        if (!submission_label.empty())
            requestObj[SUBMISSION_LABEL_KEY] = submission_label;

        const std::string& payloadStr = api::objectToString(requestObj);

        req.setContentType(network::APPLICATION_JSON);
        const network::HttpResponse& resp = req.submit(payloadStr);
        const auto& responseCode = resp.responseCode();
        if (responseCode == network::HTTP_OK) {
            return getGroupIdFromResponse(resp);
        }

    } catch (const std::exception& e) {
        const std::string& msg = std::string(e.what()) + "; url: " + url;
        ARRAS_ERROR(log::Id("postFarmHostRequest") << msg);
    }
    return BAD_GROUP_ID;
}

bool
FarmHostsForArrasUtils::cancelGroup(int groupId) const
{
    std::string msg;
    std::string url(mBaseUrl);
    url += std::string(CANCEL_PATH) + std::to_string(groupId);
    try {
        network::HttpRequest req(url, network::DELETE);
        req.setUserAgent(USER_AGENT);
        const network::HttpResponse& resp = req.submit();
        std::string respStr;
        resp.getResponseString(respStr);
        api::Object obj;
        api::stringToObject(respStr, obj);
        // good status format: "error": {"errorCode": 0} 
        if (obj.isMember("error")) {
            if (obj["error"].isMember("errorCode")) {
                const api::Object& code = obj["error"]["errorCode"];
                if (code.isInt() && code.asInt() == 0) {
                    return true;
                }
            }
        }
        ARRAS_ERROR(log::Id("cancelGroup") << respStr);
    } catch (const std::exception& e) {
        msg = std::string(e.what()) + "; url: " + url;
        ARRAS_ERROR(log::Id("cancelGroup") << msg);
    }
    return false;
}

bool
FarmHostsForArrasUtils::setRunLimit(int groupId, float hours) const
{
    // build url
    std::ostringstream oss;
    oss << mBaseUrl << RUN_LIMIT_PATH << groupId << "/" << hours;
    const std::string& url = oss.str();
    std::string msg;
    try {
        network::HttpRequest req(url, network::PUT);
        req.setUserAgent(USER_AGENT);

        api::Object requestObj;
        requestObj[MINUTES_KEY] = (int)(hours * 60);
        const std::string& payloadStr = api::objectToString(requestObj);
        const network::HttpResponse& resp = req.submit(payloadStr);
        std::string respStr;
        resp.getResponseString(respStr);
        api::Object obj;
        api::stringToObject(respStr, obj);
        // good status format: "error": {"errorCode": 0}
        if (obj.isMember("error")) {
            if (obj["error"].isMember("errorCode")) {
                const api::Object& code = obj["error"]["errorCode"];
                if (code.isInt() && code.asInt() == 0) {
                    return true;
                }
            }
        }
        ARRAS_ERROR(log::Id("setRunLimit") << respStr);
    } catch (const std::exception& e) {
        msg = std::string(e.what()) + "; url: " + url;
        ARRAS_ERROR(log::Id("setRunLimit") << msg);
    }
    return false;
}

bool
FarmHostsForArrasUtils::setEnv(const std::string& env) const
{
    std::string msg;
    std::string url(mBaseUrl);
    url += std::string(ENV_PATH) + env;
    try {
        network::HttpRequest req(url, network::PUT);
        req.setUserAgent(USER_AGENT);
        api::Object requestObj;
        requestObj["env"] = env;
        const std::string& payloadStr = api::objectToString(requestObj);
        req.setContentType(network::APPLICATION_JSON);
        const network::HttpResponse& resp = req.submit(payloadStr);
        const auto& responseCode = resp.responseCode();
        if (responseCode == network::HTTP_OK) {
            return true;
        } else {
            msg = "unexpected response code: " + std::to_string(responseCode);
            ARRAS_ERROR(log::Id("FarmHostsForArrasUtils::setEnv") << msg);
        }
    } catch (const std::exception& e) {
        msg = std::string(e.what()) + "; url: " + url;
        ARRAS_ERROR(log::Id("FarmHostsForArrasUtils::setEnv") << msg);
    }
    return false;
}

api::Object
FarmHostsForArrasUtils::getObjectFromResponse(const network::HttpResponse& resp)
{
    std::string respStr;
    resp.getResponseString(respStr);

    api::Object obj;
    api::stringToObject(respStr, obj);
    return obj;
}

int
FarmHostsForArrasUtils::getGroupIdFromResponse(const network::HttpResponse& resp)
{
    const api::Object& obj = getObjectFromResponse(resp);
    if (obj.isObject()) {
        if (obj.isMember(GROUP_ID_KEY)) {
            const api::Object& gId = obj[GROUP_ID_KEY];
            if (gId.isInt()) {
		int groupId = gId.asInt();
                return groupId;
            }
        }
    }
    return BAD_GROUP_ID;
}

std::string
FarmHostsForArrasUtils::getBaseUrl(const std::string& datacenter)
{
    // First get silo url from config
    std::string silo_url = getSiloUrl(datacenter);
    if (silo_url.empty()) return "";

    // Then get multi-session-handler port from silo
    // 1) silo must be running
    // 2) multi-session-handler must be running under silo management
    std::string url = silo_url + SILO_PROCESSES;
    try {
        // get all processes managed by silo
        network::HttpRequest req(url);
        req.setUserAgent(USER_AGENT);
        const network::HttpResponse& resp = req.submit();
        const auto& responseCode = resp.responseCode();
        if (responseCode == network::HTTP_OK) {

            const api::Object& obj = getObjectFromResponse(resp);
            if (obj.isArray()) {

                Json::ArrayIndex size = obj.size();
                for (Json::ArrayIndex i = 0; i < size; i++) {

                    // Find the base process name and RUNNING.
                    // There should at most be 1.
                    const api::Object& elem = obj[i];
                    if (elem["base_process_name"].asString() == "multi_session_request_handler" &&
                        elem["process_state"].asString() == "RUNNING") {

                        // expecting 2nd token in command line to be the port
                        const std::string& cmd = elem["original_command_line"].asString();

                        std::size_t pos = cmd.find(" ");
                        if (pos != std::string::npos) {
                            // This is the port.
                            const std::string& port = cmd.substr(pos+1);
                            // This is the url.
                            const std::string baseUrl = "http://localhost:" + port;
                            return baseUrl;
                        }
                    }
                }
            }
        } else {
            const std::string& msg = "unexpected response code: " + std::to_string(responseCode);
            ARRAS_ERROR(log::Id("getFarmHostDetail") << msg);
        }
    } catch (const std::exception& e) {
        const std::string& msg = std::string(e.what()) + "; url: " + url;
        ARRAS_ERROR(log::Id("getFarmHostDetail") << msg);
    }
    return "";
}

std::string
FarmHostsForArrasUtils::getSiloUrl(const std::string& datacenter)
{
    // First get config service base url from env.
    const char* configSrvUrl = std::getenv(DWA_CONFIG_ENV_NAME);
    if (configSrvUrl == nullptr) {
        const std::string& msg = " Undefined environment: " +
            std::string(DWA_CONFIG_ENV_NAME);
        ARRAS_ERROR(log::Id("getSiloUrl") << msg);
        return "";
    }

    // Build config service endpoint to get silo.
    std::string url(configSrvUrl);
    url += "serve/";
    url += datacenter;
    url += SILO_PATH;
    try {
        network::HttpRequest req(url);
        req.setUserAgent(USER_AGENT);
        const network::HttpResponse & resp = req.submit();
        const auto& responseCode = resp.responseCode();
        if (responseCode == network::HTTP_OK) {
            std::string respStr;
            resp.getResponseString(respStr);
            return respStr;
        } else {
            const std::string& msg = "Error response code: " +
            std::to_string(responseCode) + ", url: " + url;
            ARRAS_ERROR(log::Id("getSiloUrl") << msg);
        }
    } catch (const std::exception& e) {
        const std::string& msg = std::string(e.what()) + "; url: " + url;
        ARRAS_ERROR(log::Id("getSiloUrl") << msg);
    }
    return "";
}

} // namespace sdk
} // namespae arras4


