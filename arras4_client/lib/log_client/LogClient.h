// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/**
 * Arras Log Client : simple client for arras log service
 */

#ifndef __ARRAS4_CLIENT_LOGCLIENT_H_
#define __ARRAS4_CLIENT_LOGCLIENT_H_

#include <http/HttpRequest.h>
#include <message_api/Object.h>

#include <exception>
#include <memory>
#include <string>


//class TestLogClient;


namespace arras4 {
namespace log_client {

/**
 * Simple struct for a log record
 */
struct LogRecord
{
    std::string id;             // arras logs id
    std::string sessionId;
    std::string hostname;
    std::string loglevel;
    std::string output;
    std::string processname;
    std::string timestamp;
    std::string thread;
    std::string pid;
    std::string timestampEpochUsecs;

    std::string getLogLine() const;  // reconstruct the log(line) from this record
};

typedef std::vector<LogRecord> LogRecords;


/**
 * LogClientException
 */
class LogClientException: public std::exception
{
public:
    explicit LogClientException(const char* msg) : std::exception(), mWhat(msg) {}
    explicit LogClientException(const std::string& msg) : std::exception(), mWhat(msg) {}

    const char* what() const throw() { return mWhat.c_str(); }

private:
  std::string mWhat;

};


class LogClient
{
public:
    /**
     *  datacenter: "gld", "las"
     *  environment: "prod", "uns", "stb"
     */
    LogClient(const std::string& datacenter,
              const std::string& environment);

    /**
     * construct from a given arrasLogServiceBaseUrl:
     *   example: "http://arraslogs-0-0-3-0-show-uns.b-ose.gld.dreamworks.net:80/arraslogs/logs"
     */
    explicit LogClient(const std::string& arrasLogServiceBaseUrl);

    ~LogClient();

    const std::string& getBaseUrl() const { return mBaseUrl; }

    /**
     * About start/end time parameters used in following queries:
     *
     * start: input string, start time of records. Timing not used in query when empty.
     *    Expected format: "2018-04-12T00:00:00.000Z", "2018-04-11T00:00:00.000-07:00"
     *    Defaults to empty.
     *
     * end: input string, end time of records; works with start time; end is optional.
     *    when start is specified and end is empty, get 1 page of records from start.
     *    expected format: "2018-04-12T00:00:00.000Z", "2018-04-11T00:00:00.000-07:00"
     *    Defaults to empty.
     */

    /**
     * About pagination parameters used in following queries:
     *
     * pageIndex: input int, page number, must work with non-zero recordsPerPage.
     *    Pagination not used in query when both pageIndex and recordsPerPage are 0.
     *    Defaults to 0.
     *
     * recordsPerPage: input int, number of recordes per page.
     *    If 0, no paganation.  Defaults to 0.
     */

    /**
     * ascendingOrder: input bool, when true records would be
     * chronological order based on timestamp.
     * When false, in reverse chronological order.
     */

    /**
     * Fetch log records associated by hostname from arras log service.
     *
     * @param hostname, input string, hostname
     *    examples: "ih0037.gld.dreamworks.net"
     *
     * Please see above for pagination and time parameters.
     */
    std::unique_ptr<LogRecords> getLogsByHostname(
        const std::string& hostname,
        const std::string& start = std::string(),
        const std::string& end = std::string(),
        int pageIndex = 0,
        int recordsPerPage = 0,
        bool ascendingOrder = true);

    /**
     * Fetch log records associated by a sessionId from arras log service.
     *
     * @param sessionId, input string, arras session id
     *
     * Please see above for pagination and time parameters.
     */
    std::unique_ptr<LogRecords> getLogsBySessionId(
        const std::string& sessionId,
        const std::string& start = std::string(),
        const std::string& end = std::string(),
        int pageIndex = 0,
        int recordsPerPage = 0,
        bool ascendingOrder = true);

    /**
     * Fetch log records associated by a sessionId and a compName
     * from arras log service.
     *
     * @param sessionId, input string, arras session id
     *
     * @param compName, input string, like in "LogRecord.processname"
     *    examples: "comp-mcrt", "comp-merge"
     *
     * Please see above for pagination parameters.
     */
    std::unique_ptr<LogRecords> getLogsBySessionIdAndCompName(
        const std::string& sessionId,
        const std::string& compName,
        int pageIndex = 0,
        int recordsPerPage = 0,
        bool ascendingOrder = true);

    /**
     * Fetch log records associated by a sessionId and a compName
     * from arras log service.
     *
     * @param sessionId, input string, arras session id
     *
     * @param compName, input string, like in "LogRecord.processname"
     *    examples: "comp-mcrt", "comp-merge"
     *
     * Please see above for pagination and time parameters.
     */
    std::unique_ptr<LogRecords> getLogsBySessionIdAndCompName(
        const std::string& sessionId,
        const std::string& compName,
        const std::string& start = std::string(),
        const std::string& end = std::string(),
        unsigned int pageIndex = 0,
        unsigned int recordsPerPage = 0,
        bool ascendingOrder = true);

    /**
     * Get full session logs by sessionId.
     *
     * @param sessionId, input string, arras session id
     *
     * Please see above for ascendingOrder.
     */
    std::unique_ptr<LogRecords> getLogsBySessionIdComplete(
        const std::string& sessionId,
        bool ascendingOrder = true);

    /**
     * Get full session logs by sessionId and compName
     *
     * @param sessionId, input string, arras session id
     *
     * @param compName, input string,
     *    examples: "comp-mcrt", "comp-merge"
     *
     * Please see above for ascendingOrder.
     */
    std::unique_ptr<LogRecords> getLogsBySessionIdAndCompNameComplete(
        const std::string& sessionId,
        const std::string& compName,
        bool ascendingOrder = true);


    /**
     * Tail session logs by sessionId and compName
     *
     * @param sessionId, input string, arras session id
     *
     * @param compName, input string,
     *    examples: "comp-mcrt", "comp-merge", or ""
     *    Note: when compName is "", returns whole session logs
     *
     * @param start, input string,
     *    start time stamp. example: "2019-07-11T00:00:00.000-07:00"
     */
    std::unique_ptr<LogRecords> tail(
        const std::string& sessionId,
        const std::string& compName,
        const std::string& start = std::string());


    /**
     * Simple Util to make a http GET and returns a http response.
     * Throws LogClientException when errors.
     *
     * @param url, input string, url path.
     */
    static network::HttpResponse fetch(const std::string& url);

    /**
     * Util for a http GET and return an api::Object (i.e. Json::Value)
     * Expecting http response in json format.
     * Throws LogClientException.
     *
     * @param url, input string, url path.
     */
    static std::unique_ptr<api::Object> fetchObject(const std::string& url);

private:

    static std::string buildUrl(
        const std::string& baseUrl,
        const std::string& start,
        const std::string& end,
        int pageIndex,
        int recordsPerPage,
        bool ascendingOrder);

    static const std::string getArrasLogsUrl(
        const std::string& datacenter,
        const std::string& environment);

    static std::unique_ptr<LogRecords> objectToLogRecords(
        std::unique_ptr<api::Object>);

    static void objectToLogRecord(
        LogRecord& logRecord, 
        const api::Object& obj);

    static bool objectIsLogRecord(
        const api::Object& obj);

    std::string mBaseUrl;
};


} // end namespace log_client
} // end namespace arras

#endif /* __ARRAS4_CLIENT_LOGCLIENT_H_ */
