// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "LogClient.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>
#include <http/HttpResponse.h>

#include <boost/algorithm/string.hpp>
#include <json/json.h>
#include <ctime>
#include <sstream>

namespace {
const char* DWA_CONFIG_ENV_NAME = "DWA_CONFIG_SERVICE";
const char* ARRAS_CONFIG_PATH = "serve/jose/arras/endpoints/";
const char* BY_HOSTNAME_PATH = "/logs/host/";
const char* BY_SESSIONID_PATH = "/logs/session/";
const char* BY_COMPLETE_SESSION_PATH = "/logs/complete/session/";
const char* USER_AGENT = "arras log client";

}  // unnamed namespace


namespace arras4 {
namespace log_client {

LogClient::LogClient(const std::string& datacenter,
                     const std::string& environment) :
    mBaseUrl(getArrasLogsUrl(datacenter, environment))
{
}

LogClient::LogClient(const std::string& arrasLogServiceBaseUrl) :
    mBaseUrl(arrasLogServiceBaseUrl)
{
}

LogClient::~LogClient()
{
}


std::unique_ptr<LogRecords>
LogClient::getLogsByHostname(
    const std::string& hostname,
    const std::string& start,
    const std::string& end,
    int pageIndex,
    int recordsPerPage,
    bool ascendingOrder)
{
    // Build the url
    std::string url(mBaseUrl);
    url += BY_HOSTNAME_PATH;
    url += hostname;
    url = buildUrl(url, start, end, pageIndex, recordsPerPage, ascendingOrder);

    try {
        return LogClient::objectToLogRecords(fetchObject(url));

    } catch (std::exception& e) {
        const std::string& msg = std::string(e.what()) + ", url: " + url;
        ARRAS_ERROR(log::Id("getLogsByHostname") << msg);
        throw LogClientException("getLogsByHostname: " + msg);
    }
}

std::unique_ptr<LogRecords>
LogClient::getLogsBySessionId(
    const std::string& sessionId,
    const std::string& start,
    const std::string& end,
    int pageIndex,
    int recordsPerPage,
    bool ascendingOrder)
{
    // Build the url
    std::string url(mBaseUrl);
    url += BY_SESSIONID_PATH;
    url += sessionId;
    url = buildUrl(url, start, end, pageIndex, recordsPerPage, ascendingOrder);

    try {
        return objectToLogRecords(fetchObject(url));

    } catch (std::exception& e) {
        const std::string& msg = std::string(e.what()) + ", url: " + url;
        ARRAS_ERROR(log::Id("getLogsBySessionId") << msg);
        throw LogClientException("getLogsBySessionId: " + msg);
    }
}

std::unique_ptr<LogRecords>
LogClient::getLogsBySessionIdAndCompName(
    const std::string& sessionId,
    const std::string& compName,
    int pageIndex,
    int recordsPerPage,
    bool ascendingOrder)
{
    // Build the url
    std::string url(mBaseUrl);
    url += BY_SESSIONID_PATH;
    url += sessionId + "/" + compName;
    url = buildUrl(url, "", "", pageIndex, recordsPerPage, ascendingOrder);
    try {
        return objectToLogRecords(fetchObject(url));

    } catch (std::exception& e) {
        const std::string& msg = std::string(e.what()) + ", url: " + url;
        ARRAS_ERROR(log::Id("getLogsBySessionIdAndCompName") << msg);
        throw LogClientException("getLogsBySessionIdAndCompName: " + msg);
    }
}

std::unique_ptr<LogRecords>
LogClient::getLogsBySessionIdAndCompName(
    const std::string& sessionId,
    const std::string& compName,
    const std::string& start,
    const std::string& end,
    unsigned int pageIndex,
    unsigned int recordsPerPage,
    bool ascendingOrder)
{
    // Build the url
    std::string url(mBaseUrl);
    url += BY_SESSIONID_PATH;
    url += sessionId + "/" + compName;
    url = buildUrl(url, start, end, static_cast<int>(pageIndex), static_cast<int>(recordsPerPage), ascendingOrder);
    try {
        return objectToLogRecords(fetchObject(url));

    } catch (const std::exception& e) {
        const std::string& msg = std::string(e.what()) + ", url: " + url;
        ARRAS_ERROR(log::Id("getLogsBySessionIdAndCompName") << msg);
        throw LogClientException("getLogsBySessionIdAndCompName: " + msg);
    }
}


std::unique_ptr<LogRecords>
LogClient::getLogsBySessionIdComplete(
    const std::string& sessionId,
    bool ascendingOrder)
{
    // Build the url
    std::string url(mBaseUrl);
    url += BY_COMPLETE_SESSION_PATH;
    url += sessionId;
    url = buildUrl(url, "", "", 0, 0, ascendingOrder);
    std::string respStr;

    // Make the rest call.
    try {
        const network::HttpResponse& resp = fetch(url);
        resp.getResponseString(respStr);
    } catch (std::exception& e) {
        const std::string& msg = std::string(e.what()) + ", url: " + url;
        ARRAS_ERROR(log::Id("getLogsBySessionIdComplete") << msg);
        throw LogClientException("getLogsBySessionIdComplete: " + msg);
    }

    // Response string format: lines of records, with '\n' as delimiter.
    // So we break the response string into lines and turn each line into a LogRecord.
    std::size_t pos = 0;
    std::unique_ptr<LogRecords> records(new LogRecords);
    while (pos < respStr.size()) {

        std::size_t end = respStr.find('\n', pos);
        if (end == std::string::npos) break;  // no more lines

        const std::string& line = respStr.substr(pos, end-pos);
        try {
	    
            pos = end + 1;
            if (line.size() > 0) {
                api::Object obj;
                api::stringToObject(line, obj);  // expecting line to be in json format
                if (objectIsLogRecord(obj)) {
                    (*records).push_back(LogRecord());
                    LogRecord& logRecord  = (*records).back();
                    objectToLogRecord(logRecord, obj);
                } else {
                    ARRAS_WARN(log::Id("getLogsBySessionIdComplete") << " invalid line: " << line);
                }
            }
        } catch (std::exception& e) {
            const std::string& msg = std::string(e.what()) + ", line: " + line;
            ARRAS_ERROR(log::Id("getLogsBySessionIdComplete") << msg);
            records.reset(nullptr);
            throw LogClientException("getLogsBySessionIdComplete: " + msg);
        } 
    } 
    return std::move(records);
}


std::unique_ptr<LogRecords>
LogClient::getLogsBySessionIdAndCompNameComplete(
    const std::string& sessionId,
    const std::string& compName,
    bool ascendingOrder)
{
    // Build the url
    std::string url(mBaseUrl);
    url += BY_COMPLETE_SESSION_PATH;
    url += sessionId + "/" + compName;
    url = buildUrl(url, "", "", 0, 0, ascendingOrder);
    std::string respStr;

    // Make the rest call.
    try {
        const network::HttpResponse& resp = fetch(url);
        resp.getResponseString(respStr);
    } catch (std::exception& e) {
        const std::string& msg = std::string(e.what()) + ", url: " + url;
        ARRAS_ERROR(log::Id("getLogsBySessionIdAndCompNameComplete") << msg);
        throw LogClientException("getLogsBySessionIdAndCompNameComplete: " + msg);
    }

    // Response string format: lines of records, with '\n' as delimiter.
    // So we break the response string into lines and turn each line into a LogRecord.
    std::size_t pos = 0;
    std::unique_ptr<LogRecords> records(new LogRecords);
    while (pos < respStr.size()) {

        std::size_t end = respStr.find('\n', pos);
        if (end == std::string::npos) break;  // no more lines

        const std::string& line = respStr.substr(pos, end-pos);
        try {

            pos = end + 1;
            if (line.size() > 0) {
                api::Object obj;
                api::stringToObject(line, obj);  // expecting line to be in json format
                if (objectIsLogRecord(obj)) {
                    (*records).emplace_back(LogRecord());
                    LogRecord& logRecord  = (*records).back();
                    objectToLogRecord(logRecord, obj);
                } else {
                    ARRAS_WARN(log::Id("getLogsBySessionIdAndCompNameComplete") << " invalid line: " << line);
                }
            }
        } catch (std::exception& e) {
            const std::string& msg = std::string(e.what()) + ", line: " + line;
            ARRAS_ERROR(log::Id("getLogsBySessionIdAndCompNameComplete") << msg);
            records.reset(nullptr);
            throw LogClientException("getLogsBySessionIdAndCompNameComplete: " + msg);
        }
    }
    return std::move(records);
}


std::unique_ptr<LogRecords>
LogClient::tail(
     const std::string& sessionId,
     const std::string& compName,
     const std::string& start)
{
    // if no start timestamp, fetch complete logs
    if (start.empty()) {
        if (compName.empty())
            return getLogsBySessionIdComplete(sessionId);
        else
            return getLogsBySessionIdAndCompNameComplete(sessionId, compName);
    } else {
        // fetch from start time
        if (compName.empty())
            return getLogsBySessionId(sessionId, start);
        else
            return getLogsBySessionIdAndCompName(sessionId, compName, start);
    }
}


network::HttpResponse
LogClient::fetch(const std::string& url)
{
    network::HttpRequest req(url);
    req.setUserAgent(USER_AGENT);
    const network::HttpResponse& resp = req.submit();
    const auto& responseCode = resp.responseCode();
    if (responseCode < network::HTTP_OK ||
        responseCode >= network::HTTP_MULTIPLE_CHOICES) {

        std::string msgStr;
        resp.getResponseString(msgStr);  // error message
        const std::string& msg = "Error response code: " +
            std::to_string(responseCode) + msgStr + ", url: " + url;
        ARRAS_ERROR(log::Id("fetch") << msg);
        throw LogClientException("fetch: " + msg);
    }

    return resp;
}


std::unique_ptr<api::Object>
LogClient::fetchObject(const std::string& url)
{
    try {
        const auto & resp = fetch(url);
        std::string respStr;
        resp.getResponseString(respStr);
        std::unique_ptr<api::Object> results(new api::Object);
        api::stringToObject(respStr, *results);  // expecting string to be in json

        return std::move(results);

    } catch (std::exception& e) {
        const std::string& msg = std::string(e.what()) + "; url: " + url;
        ARRAS_ERROR(log::Id("fetchObject") << msg);
        throw LogClientException("fetchObject: " + msg);
    }
}

const std::string
LogClient::getArrasLogsUrl(const std::string& datacenter, const std::string& environment)
{
    const char* configSrvUrl = std::getenv(DWA_CONFIG_ENV_NAME);
    if (configSrvUrl == nullptr) {
        const std::string& msg = " Undefined environment: " +
            std::string(DWA_CONFIG_ENV_NAME);
        ARRAS_ERROR(log::Id("getArrasLogsUrl") << msg);
        throw LogClientException("getArrasLogsUrl: " + msg);
    }

    // Build the resource url
    std::string url(configSrvUrl);
    url += ARRAS_CONFIG_PATH;
    url += datacenter;
    url += "/";
    url += environment;
    url += "/arraslogs/url";

    const network::HttpResponse & resp = fetch(url);
    std::string retVal;
    resp.getResponseString(retVal);
    ARRAS_INFO(log::Id("getArrasLogsUrl") << retVal);
    return retVal;
}

std::string
LogRecord::getLogLine() const
{
    // for sessionId is null(i.e. there is no applicable sessionId in the log record)
    const std::string sessionIdStr = sessionId == "null" ? "" : "[" + sessionId + "]";

    // Make the line similar to the original log.
    std::ostringstream oss;
    oss << timestamp << " " << loglevel << " " << processname << "[" << pid << "]:"
        << thread << ": " << sessionIdStr << output;
    return oss.str();
}

std::unique_ptr<LogRecords>
LogClient::objectToLogRecords(std::unique_ptr<api::Object> obj)
{
    std::unique_ptr<LogRecords> records(new LogRecords);
    if (obj->isObject()) {

        // First look for the "content" key:
        if (obj->isMember("content")) {
            const api::Object& content = (*obj)["content"];

            // Expecting an array of objects:
            if (content.isArray()) {

                // For each object in the array make a log record
                for (Json::Value::const_iterator i = content.begin(); i != content.end(); ++i) {

                    const api::Object& item = *i;
                    if (objectIsLogRecord(item)) {
                        (*records).push_back(LogRecord());
                        LogRecord& logRecord = (*records).back();
                        objectToLogRecord(logRecord, item);
                    }
                }
            }
        }
    }

    obj.reset(nullptr);
    return std::move(records);
}

void
LogClient::objectToLogRecord(LogRecord& logRecord, const api::Object& obj)
{
    logRecord.id          = obj["id"].asString();
    logRecord.sessionId   = obj["sessionId"].asString();
    logRecord.hostname    = obj["hostname"].asString();
    logRecord.loglevel    = obj["loglevel"].asString();
    logRecord.output      = obj["output"].asString();
    logRecord.processname = obj["processname"].asString();
    logRecord.timestamp   = obj["timestamp"].asString();
    logRecord.thread      = obj["thread"].asString();
    logRecord.pid         = obj["pid"].asString();
    logRecord.timestampEpochUsecs = obj["timestampEpochUsecs"].asString();
}

bool
LogClient::objectIsLogRecord(const api::Object& obj)
{
    return obj.isMember("id") && 
        obj.isMember("sessionId") &&
        obj.isMember("hostname") &&
        obj.isMember("loglevel") &&
        obj.isMember("output") &&
        obj.isMember("processname") &&
        obj.isMember("timestamp") &&
        obj.isMember("thread") &&
        obj.isMember("pid") &&
        obj.isMember("timestampEpochUsecs");
}

std::string
LogClient::buildUrl(
    const std::string& baseUrl,
    const std::string& start,
    const std::string& end,
    int pageIndex,
    int recordsPerPage,
    bool ascendingOrder)
{
    std::string url(baseUrl);

    // Check if we have page/size and start/end time stuff.
    const bool
        pagination = recordsPerPage > 0,
        useTime = !start.empty();

    url += "?";

    // ascending order
    url += ascendingOrder ? "sort=asc" : "sort=desc";

    // append page and size stuff
    if (pagination) {
        url += "&page=";
        url += std::to_string(pageIndex);
        url += "&size=";
        url += std::to_string(recordsPerPage);
    }

    // start is specified.
    if (useTime) {
        // url encode for start-time
        std::string s(start);
        boost::replace_all(s, ":", "%3A");
        boost::replace_all(s, " ", "%20");
        boost::replace_all(s, "+", "%2B");
        url += "&start=" + s;

        // end is optional.
        if (!end.empty()) {
            // url encode for end-time
            std::string e(end);
            boost::replace_all(e, ":", "%3A");
            boost::replace_all(e, " ", "%20");
            boost::replace_all(e, "+", "%2B");
            url += "&end=" + e;
        }
    }

    return url;
}

} // namespace log_client
} // namespae arras

