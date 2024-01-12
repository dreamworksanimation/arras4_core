// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "TestLogClient.h"

#include <message_api/Object.h>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

namespace {
// JIRA BDA-344 : timestamp sort out of order when sub-second digit is 0
// Tests would fail until the BDA-344 is fixed.
// BDA-344 has been fixed.  So we can compare time now!
//#define SKIP_TIME_COMPARE

// debug print
#define TEST_LOG_CLIENT_PRINT_NUM_RECORDS

// use uns arras log service
//#define TEST_AGAINST_UNS_STACK

const char* DATA_CENTER = "gld";
#ifdef TEST_AGAINST_UNS_STACK
// Targeting uns arras log service.
const char* STACK = "uns";  
const char* CONFIG_BASE = "http://uns.svc.gld.dreamworks.net/pam/config/1/";
const char* ARRAS_LOG_SERVICE_URL = "http://uns.svc.gld.dreamworks.net/jose/arraslogs/1/";
const char* COORD = "serve/jose/arras/endpoints/gld/uns/coordinator/url";
#else
// Targeting prod arras log service.
const char* STACK = "prod";  
const char* CONFIG_BASE = "http://studio.svc.gld.dreamworks.net/pam/config/1/";
const char* ARRAS_LOG_SERVICE_URL = "http://studio.svc.gld.dreamworks.net/jose/arraslogs/1/";
const char* COORD = "serve/jose/arras/endpoints/gld/prod/coordinator/url";
#endif



// Returns true if text ends with the "+hh:mm", or "-hh:mm" time zone format
bool checkTZFormat(const std::string& text) {
    if (text.length() < 6) return false;
    std::size_t pos = text.length() - 6;
    if ((text.at(pos) == '+' || text.at(pos) == '-') && text.at(pos+3) == ':')
        return true;
    else
        return false;
}

// Time zone designator(TZD) format should be (Z or +hh:mm or -hh:mm)
void fixTZD(std::string& text) {
    // if format is "+hhmm" or "-hhmm" change to "+hh:mm" or "-hh:mm"
    if (text.length() >= 5) {
        std::size_t pos = text.length() - 5;
        if (text.at(pos) == '+' || text.at(pos) == '-') {
            // inset ":" for hh:mm format
            text.insert(pos + 3, ":"); 
        }
    }
}

}  // unnamed namespace


CPPUNIT_TEST_SUITE_REGISTRATION(TestLogClient);

void
TestLogClient::setUp()
{
    setenv("DWA_CONFIG_SERVICE", CONFIG_BASE, true);
    logClient.reset(new arras4::log_client::LogClient(DATA_CENTER, STACK));

    // Get uns coordinator base url from studio config service
    std::string coordUrl, cUrl(CONFIG_BASE);
    cUrl += COORD;
    try {
        auto resp = arras4::log_client::LogClient::fetch(cUrl);
        resp.getResponseString(coordUrl);

    } catch (std::exception& e) {

        std::cerr << "Error TestLogClient::setUp: getting coordinator url " 
            << e.what() << ", url: " << cUrl << "\n";
    }

    // Try to get a sessionId from uns coordinator history endpoint.
    const char* days[] = { "one", "two", "five"};
    std::string tmpUrl, base = coordUrl + "/history/sessions?days=";
    try {
        for (size_t i = 0; i < 3; ++i) {
            tmpUrl = base + days[i];
            std::unique_ptr<arras4::api::Object> obj =
                arras4::log_client::LogClient::fetchObject(tmpUrl);
            if (obj && obj->isArray()) {
                for (auto it = obj->begin(); it != obj->end(); ++it) {
                    const arras4::api::Object& item = *it;
                    if (!item.isMember("id")) continue;
                    if (!item.isMember("start")) continue;
                    if (!item.isMember("sessionconfig")) continue;

                    sessionId    = item["id"].asString();   
                    sessionStart = item["start"].asString();   
                    sessionEnd   = item["end"].asString();   
                    sessionNode  = item["entryNodeName"].asString();
                    const arras4::api::Object& val = item["sessionconfig"];
                    const std::vector<std::string>& keys = val.getMemberNames();
                    for (size_t k = 0; k < keys.size(); k++) {
                        const std::string& key = keys[k];
                        if (key != "(client)") {
                            sessionCompName = key;
                            break;
                        }  
                    }
                    if (!sessionId.empty() && !sessionStart.empty() && !sessionCompName.empty())
                        break;
                }
            }
            obj.reset(nullptr);

            // Expecting Time zone designator(TZD) format to be (Z or +hh:mm or -hh:mm)
            fixTZD(sessionStart);
            fixTZD(sessionEnd);  
            if (!sessionId.empty() && !sessionStart.empty())
                break;           
        }
    } catch (std::exception& e) {

        std::cerr << "Error TestLogClient::setUp: " << e.what() << ", url: " << tmpUrl << "\n";
    }

    if (sessionId.empty() && sessionStart.empty() && sessionEnd.empty()) {
        std::cerr << "Error TestLogClient::setUp: no valid session from '/history' endpoint: '"
            << sessionId << "' '" << sessionNode << "' '" << sessionStart << "' '" 
            << sessionStart << "'\n";
    }

    // Try to get host from coordinator nodes endpoint if needed.
    if (!sessionNode.empty()) return;

    tmpUrl = coordUrl + "/nodes";
    try {
        std::unique_ptr<arras4::api::Object> obj =
            arras4::log_client::LogClient::fetchObject(tmpUrl);
        if (obj && obj->isArray()) {
            for (auto i = obj->begin(); i != obj->end(); ++i) {
                const arras4::api::Object& item = *i;
                sessionNode = item["hostname"].asString();
                if (!sessionNode.empty())
                    break;
            }
            obj.reset(nullptr);
        }
    } catch (std::exception& e) {

        std::cerr << "Error TestLogClient::setUp: " << e.what() << ", url: " << tmpUrl << "\n";
    }

    if (sessionNode.empty()) {
        std::cerr << "Error TestLogClient::setUp: no valid node from '/nodes' endpoint: '"
            << tmpUrl << "'\n";
    }   
}

void
TestLogClient::tearDown()
{
    logClient.reset(nullptr);
}

void
TestLogClient::testGetByHost()
{
    std::string host = sessionNode;
    CPPUNIT_ASSERT(!host.empty());

    std::unique_ptr<arras4::log_client::LogRecords> result;
    try {
        result = logClient->getLogsByHostname(host);

    } catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetByHost() ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }

    std::string last, current;
    const arras4::log_client::LogRecords& records = (*result);

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
    std::cerr << "\ntestGetByHost A num records: " << records.size() << std::endl;
#endif

    for (size_t i = 0; i < records.size(); i++) {

        // verify each record is from the right host
        CPPUNIT_ASSERT(records[i].hostname == host);

#ifndef SKIP_TIME_COMPARE
        // verify ascending order
        if (!last.empty()) {
	        CPPUNIT_ASSERT_MESSAGE(last + " NOT<= " + records[i].timestamp,
                last.compare(records[i].timestamp) <= 0);
        }
#endif
        last = records[i].timestamp;
        CPPUNIT_ASSERT(last.empty() == false);
    }
    result.reset(nullptr);
    last.clear();

    // Constructy client from the arras log service url.
    arras4::log_client::LogClient client(ARRAS_LOG_SERVICE_URL);
    host = sessionNode;
    CPPUNIT_ASSERT(!host.empty());

    try {
        result = client.getLogsByHostname(host);

    } catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetByHost(): "); 
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }

    const arras4::log_client::LogRecords& records_2 = (*result);

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
    std::cout << "\ntestGetByHost B num records: " << records_2.size() << std::endl;
#endif

    for (size_t i = 0; i < records_2.size(); i++) {

        const std::string& line = records_2[i].getLogLine();
        CPPUNIT_ASSERT(line.empty() == false);

        // verify each record is from the right host
        CPPUNIT_ASSERT(records_2[i].hostname == host);

        current = records_2[i].timestamp;

#ifndef SKIP_TIME_COMPARE
        // verify ascending order
        if (!last.empty()) {
            CPPUNIT_ASSERT(last.compare(current) <= 0);
        }
#endif

        last = current;
    }
    result.reset(nullptr);
}

void
TestLogClient::testGetByHostDesc()
{
    const std::string host = sessionNode;
    CPPUNIT_ASSERT(!host.empty());

    std::unique_ptr<arras4::log_client::LogRecords> result;
    try {
        result = logClient->getLogsByHostname(host, "", "", 0, 0, /*ascendingOrder*/false);

    }  catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetByHostDesc(): ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }

    std::string last;
    const arras4::log_client::LogRecords& records = (*result);

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
    std::cout << "\ntestGetByHostDesc num records: " << records.size() << std::endl;
#endif

    for (size_t i = 0; i < records.size(); i++) {

        // verify each record is from the right host
        CPPUNIT_ASSERT(records[i].hostname == host);

#ifndef SKIP_TIME_COMPARE
        // verify descending order, so last timestamp should be >= current timestamp
        if (!last.empty())
            CPPUNIT_ASSERT_MESSAGE(last + " NOT>= " + records[i].timestamp,
                last.compare(records[i].timestamp) >= 0);
#endif
        last = records[i].timestamp;
        CPPUNIT_ASSERT(last.empty() == false);
    }

    result.reset(nullptr);
}

void
TestLogClient::testGetByHostWithPage()
{
    CPPUNIT_ASSERT(sessionNode.empty() == false);
    try {
        const std::string& host = sessionNode;
        const int pageIdx = 1, recordsPerPage = 3;

        std::unique_ptr<arras4::log_client::LogRecords> result = 
            logClient->getLogsByHostname(host, "", "", pageIdx, recordsPerPage);

        const arras4::log_client::LogRecords& records = (*result);

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
        std::cout << "\ntestGetByHostWithPage num records: " << records.size() << std::endl;
#endif

        CPPUNIT_ASSERT(records.size() <= 3);  // 3 per records page
        for (size_t i = 0; i < records.size(); i++) {
            const std::string& line = records[i].getLogLine();
            //std::cout << line << "\n";
            CPPUNIT_ASSERT(line.empty() == false);
            CPPUNIT_ASSERT(records[i].hostname == host);
        }
        result.reset(nullptr);

    } catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetByHostWithPage(): ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }
}

void
TestLogClient::testGetByHostWithDate()
{
    const std::string& host = sessionNode;
    std::string start, end;
    CPPUNIT_ASSERT(host.empty() == false);

    // Get a valid start time.
    try {
        std::unique_ptr<arras4::log_client::LogRecords> result1 =
            logClient->getLogsByHostname(host, "", "", 0, 10);
        const arras4::log_client::LogRecords& records = (*result1);
        CPPUNIT_ASSERT(records.size() <= 10);  // 10 records per page
        CPPUNIT_ASSERT(records.size() > 0);    // should have at least 1 record
        start = records[0].timestamp;
        end   = records[records.size() - 1].timestamp;
        result1.reset(nullptr);
    } catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetByHostWithDate() A:");
        msg += e.what();
        CPPUNIT_FAIL(msg);

    }
    CPPUNIT_ASSERT(start.empty() == false);
    CPPUNIT_ASSERT(end.empty() == false);
    std::unique_ptr<arras4::log_client::LogRecords> result;
    try {
        result = logClient->getLogsByHostname(host, start, end);
    } catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetByHostWithDate() B:");
        msg += e.what();
        CPPUNIT_FAIL(msg);

    }

    const arras4::log_client::LogRecords& records = (*result);

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
    std::cout << "\ntestGetByHostWithDate num records: " << records.size() << std::endl;
#endif

    for (size_t i = 0; i < records.size(); i++) {
        const std::string& line = records[i].getLogLine();
        //std::cout << line << "\n";
        CPPUNIT_ASSERT(line.empty() == false);
        CPPUNIT_ASSERT(records[i].hostname == host);

#ifndef SKIP_TIME_COMPARE
        CPPUNIT_ASSERT_MESSAGE(start + " NOT<= " + records[i].timestamp,
            start.compare(records[i].timestamp) <= 0);
        CPPUNIT_ASSERT_MESSAGE(end + " NOT>= " + records[i].timestamp,
            end.compare(records[i].timestamp) >= 0);
#endif
    }
    result.reset(nullptr);
}

void
TestLogClient::testGetByHostWithPageAndDate()
{
    const std::string& host  = sessionNode;
    CPPUNIT_ASSERT(host.empty() == false);

    std::string start, end;
    CPPUNIT_ASSERT(host.empty() == false);

    // Get a valid start and end time.
    std::unique_ptr<arras4::log_client::LogRecords> result;
    try {
        result = logClient->getLogsByHostname(host, "", "", 0, 10);
        const arras4::log_client::LogRecords& records = (*result);
        CPPUNIT_ASSERT(records.size() <= 10);  // 10 records per page
        start = records[0].timestamp;
        end   = records[records.size() - 1].timestamp;
        result.reset(nullptr);
    } catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetByHostWithPageAndDate() A: ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }
    CPPUNIT_ASSERT(start.empty() == false);
    CPPUNIT_ASSERT(end.empty() == false);

    const int pageIdx = 0, recordsPerPage = 6;
    try {
        result = logClient->getLogsByHostname(host, start, end, pageIdx, recordsPerPage);
    } catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetByHostWithPageAndDate() B: ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }

    const arras4::log_client::LogRecords& records = (*result);

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
    std::cout << "\ntestGetByHostWithPageAndDate num records: " << records.size() << std::endl;
#endif

    CPPUNIT_ASSERT(records.size() <= 6);  // 6 records per page
    for (size_t i = 0; i < records.size(); i++) {
        const std::string& line = records[i].getLogLine();
        //std::cout << line << "\n";
        CPPUNIT_ASSERT(line.empty() == false);
        CPPUNIT_ASSERT_MESSAGE(host + " NOT " + records[i].hostname,
            records[i].hostname == host);

#ifndef SKIP_TIME_COMPARE
        CPPUNIT_ASSERT_MESSAGE(start + " NOT<= " + records[i].timestamp,
            start.compare(records[i].timestamp) <= 0);
        if (!end.empty()) 
            CPPUNIT_ASSERT_MESSAGE(end + " NOT>= " + records[i].timestamp,
                end.compare(records[i].timestamp) >= 0);
#endif
    }

    result.reset(nullptr);
}

void
TestLogClient::testGetBySessionId()
{
    const std::string& sId = sessionId;
    CPPUNIT_ASSERT(sId.empty() == false);

    int pageIdx = 0, recordsPerPage = 100;
    size_t total = 0, exitCheck = 0;
    while (1) {
        std::unique_ptr<arras4::log_client::LogRecords> result;
        try {
            result = logClient->getLogsBySessionId(sId, "", "", pageIdx, recordsPerPage);

        } catch (arras4::log_client::LogClientException &e) {

            std::string msg("Error TestLogClient::testGetBySessionId(): ");
            msg += e.what();
            CPPUNIT_FAIL(msg);
        }

        const arras4::log_client::LogRecords& records = (*result);
        const size_t numRecords = records.size();
        total += numRecords;
        CPPUNIT_ASSERT(numRecords <= 100);  // 100 records per page

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
        std::cout << "\ntestGetBySessionId num records: " <<numRecords<<" "<<total<< std::endl;
#endif

        for (size_t i = 0; i < records.size(); i++) {
            const std::string& line = records[i].getLogLine();
            //std::cout << line << "\n";
            CPPUNIT_ASSERT(line.empty() == false);
            CPPUNIT_ASSERT(records[i].sessionId == sId);
        }
        result.reset(nullptr);
        if (numRecords == 0) 
            break;
        else
            pageIdx ++;

        exitCheck++;
        if (exitCheck > 20) break;
    }
}

void
TestLogClient::testGetBySessionIdAndCompName()
{
    // This was a kodachi_render session on prod at 2019-05-07T09:45:12171423-07:00
    //std::string sId = "f28413c9-4659-5855-a514-61f9f9d55e5b", compName = "comp-render";

    const std::string& 
        sId = sessionId,
        compName = std::string("comp-") + sessionCompName;
    CPPUNIT_ASSERT(sId.empty() == false);
    CPPUNIT_ASSERT(compName.empty() == false);

    int pageIdx = 0, recordsPerPage = 20;
    size_t total = 0, exitCheck = 0;
    while (1) {

        std::unique_ptr<arras4::log_client::LogRecords> result;
        try {
            result = logClient->getLogsBySessionIdAndCompName(sId, compName, "", "", pageIdx, recordsPerPage);
        } catch (arras4::log_client::LogClientException &e) {

            std::string msg("Error TestLogClient::testGetBySessionIdAndCompName(): ");
            msg += e.what();
            CPPUNIT_FAIL(msg);
        }

        const arras4::log_client::LogRecords& records = (*result);
        const size_t numRecords = records.size();
        total += numRecords;
        CPPUNIT_ASSERT(numRecords <= 20); // 20 records per page
        //std::cerr<< "numRecords " << numRecords << " " << total << "\n";

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
        std::cout << "\ntestGetBySessionIdAndCompName " << numRecords << " " << total << "\n";
#endif

        for (size_t i = 0; i < records.size(); i++) {
            const std::string& line = records[i].getLogLine();
            //std::cout << line << "\n";
            CPPUNIT_ASSERT(line.empty() == false);
            CPPUNIT_ASSERT(records[i].sessionId == sId);
            CPPUNIT_ASSERT(records[i].processname == compName);
        }
        result.reset(nullptr);


        if (numRecords == 0)
            break;
        else
            pageIdx++;

        exitCheck ++;
        if (exitCheck > 20) break;  // cut short the test
    }
#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
    if (total == 0) {
          std::cerr<<"\ntestGetBySessionIdAndCompName " << sId << " " << compName << "\n";
    }
#endif
    //std::cerr<<"\ntestGetBySessionIdAndCompName " << total << " " << exitCheck << "\n";
}

void
TestLogClient::testGetBySessionIdAndTime()
{
    const std::string&  sId = sessionId;
    CPPUNIT_ASSERT(sId.empty() == false);

    std::string start, end, last, id;
    size_t total = 0, exitCheck = 0;
    while (1) {

        std::unique_ptr<arras4::log_client::LogRecords> result;
        try {
            //std::cerr << "AAA '" << start << "' '" << end << "\n";
            result = logClient->getLogsBySessionId(sId, start, end, 0, 0);
        } catch (arras4::log_client::LogClientException &e) {

            std::string msg("Error TestLogClient::testGetBySessionIdAndTime(): ");
            msg += e.what();
            CPPUNIT_FAIL(msg);
        }

        const arras4::log_client::LogRecords& records = (*result);
        const size_t numRecords = records.size();
        total += numRecords;

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
        std::cout << "\ntestGetBySessionIdAndTime " << numRecords << " " << total << std::endl;
#endif

        // Note: last as start for timestamp
        // means the previous call's last record is the same record as the current query
        // first record.
        if (!id.empty() && numRecords > 0 && last == records[0].timestamp) {
            //std::cout << "Found duplicate \n";
            //CPPUNIT_ASSERT(id == records[0].id);  // the duplicate record
        }

        for (size_t i = 0; i < records.size(); i++) {

            const std::string& line = records[i].getLogLine();
            CPPUNIT_ASSERT(line.empty() == false);
            //std::cout << line << "\n";
            last = records[i].timestamp;
            id = records[i].id;
   
            CPPUNIT_ASSERT(records[i].sessionId == sId);
 
#ifndef SKIP_TIME_COMPARE
            if (!start.empty())
                CPPUNIT_ASSERT_MESSAGE(start + " NOT<= " + records[i].timestamp,
                    start.compare(records[i].timestamp) <= 0);
            if (!end.empty()) 
                CPPUNIT_ASSERT_MESSAGE(end + " NOT>= " + records[i].timestamp,
                    end.compare(records[i].timestamp) >= 0);
#endif
        }
        result.reset(nullptr);
        if (numRecords == 0) 
            break;

        if (start == last)
            break;

        exitCheck ++;
        if (exitCheck > 20) break;  // cut short the test


        char buf[256];
        std::tm tmTime;
        strptime(last.c_str(), "%FT%T", &tmTime);
        mktime(&tmTime);
        strftime(buf, sizeof(buf), "%FT%T", &tmTime);

        start = last;

        tmTime.tm_min += 3;  // add 3 minutes
        mktime(&tmTime);
        strftime(buf, sizeof(buf), "%FT%T", &tmTime);
        end = buf;
        end += ".000000";  // add precision 

        // DTZ: same format as start: e.g. "+hh:mm" or "-hh:mm", or "Z"
        if (checkTZFormat(start)) {
	      std::size_t pos = start.length() - 6;
	      end += start.substr(pos, 6);
        } else {
	      end += "Z";
        }

    }
    //std::cerr<<" \n\nTotal " << total << " " << exitCheck <<"\n";
}

void
TestLogClient::testGetBySessionIdAndTimeEmptyEndTime()
{
    const std::string& sId = sessionId;
    std::string start, last, id;
    size_t total = 0, exitCheck = 0;
    while (1) {

        // end time is optional,  so use empty end-time
        std::unique_ptr<arras4::log_client::LogRecords> result;
        try {
            //std::cerr << "BBB '" << start << "\n";
            result = logClient->getLogsBySessionId(sId, start);

        } catch (arras4::log_client::LogClientException &e) {

            std::cout << "Error TestLogClient::testGetBySessionIdAndTimeEmptyEndTime(): "
	        << e.what() << "\n";
            break;
        }

        const arras4::log_client::LogRecords& records = (*result);
        const size_t numRecords = records.size();
        total += numRecords;

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
        std::cout << "\ntestGetBySessionIdAndTimeEmptyEndTime " << numRecords << " " << total << "\n";
#endif

        // Note: last as start for timestamp
        // means the previous call's last record is the same record as the current query
        // first record.
        if (!id.empty() && numRecords > 0 && last == records[0].timestamp) {
            //std::cout << "Found duplicate \n";
            //CPPUNIT_ASSERT(id == records[0].id);  // the duplicate record
        }

        for (size_t i = 0; i < numRecords; i++) {
            const std::string& line = records[i].getLogLine();
            //std::cout << line << "\n";
            CPPUNIT_ASSERT(line.empty() == false);

            last = records[i].timestamp;
            id = records[i].id;

            CPPUNIT_ASSERT(records[i].sessionId == sId);

#ifndef SKIP_TIME_COMPARE
            if (!start.empty()) 
                CPPUNIT_ASSERT_MESSAGE(start + " NOT<= " + records[i].timestamp,
	                start.compare(records[i].timestamp) <= 0);
#endif
        }
        result.reset(nullptr);

        if (numRecords == 0) // no more records
            break;

        if (start == last) // no more records
            break;

        exitCheck ++;
        if (exitCheck > 20) break;  // cut the test short

        start = last;  // update start time
       
    }
    //std::cerr<<" \n\nTotal " << total <<"\n";
}

void
TestLogClient::testGetBySessionIdComplete()
{
    const std::string& sId = sessionId;
    CPPUNIT_ASSERT(sId.empty() == false);

    std::unique_ptr<arras4::log_client::LogRecords> result;
    try {
        result = logClient->getLogsBySessionIdComplete(sId);

    } catch (arras4::log_client::LogClientException &e) {

        std::string msg("Error TestLogClient::testGetBySessionIdComplete(): ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }

    const arras4::log_client::LogRecords& records = (*result);
    const size_t numRecords = records.size();

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
    std::cout << "\ntestGetBySessionIdComplete numRecords " << numRecords  << "\n";
#endif

    for (size_t i = 0; i < numRecords; i++) {
        const std::string& line = records[i].getLogLine();
        //std::cout << line << "\n";
        CPPUNIT_ASSERT(line.empty() == false);
        CPPUNIT_ASSERT(records[i].sessionId == sId);
    }
    result.reset(nullptr);
}

void
TestLogClient::testTail()
{
    const std::string& sId = sessionId;
    CPPUNIT_ASSERT(sId.empty() == false);
    const std::string& compName = std::string("comp-") + sessionCompName;
    //std::cerr << "testTail: " << sId << " " << compName << "\n";

    size_t total = 0, max_calls = 10;
    std::string id, last, start = std::string();
    for (size_t j = 0; j < max_calls; j++) {

        std::unique_ptr<arras4::log_client::LogRecords> result;
        try {
            result = logClient->tail(sId, compName, start);
        } catch (arras4::log_client::LogClientException &e) {
            std::string msg("Error TestLogClient::testTail(): ");
            msg += e.what();
            CPPUNIT_FAIL(msg);
        }

        const arras4::log_client::LogRecords& records = (*result);
        const size_t numRecords = records.size();
        total += numRecords;

        // Note: last as start for timestamp
        // means the previous call's last record is the same record as the current query
        // first record.
        if (!id.empty() && numRecords > 0 && last == records[0].timestamp) {
            //std::cout << "testTail: Found duplicate \n";
            CPPUNIT_ASSERT(id == records[0].id);  // the duplicate record
        }

#ifdef TEST_LOG_CLIENT_PRINT_NUM_RECORDS
        std::cout << "\ntestTail '" << start << "' " << numRecords << " " << total << std::endl;
#endif

        for (size_t i = 0; i < numRecords; i++) {

            CPPUNIT_ASSERT(records[i].sessionId == sId);
            CPPUNIT_ASSERT(records[i].processname == compName);

            const std::string& line = records[i].getLogLine();
            CPPUNIT_ASSERT(line.empty() == false);
            //std::cout << line << "\n";

            // records should be in ascending order
            CPPUNIT_ASSERT_MESSAGE(last + " NOT<= " + records[i].timestamp,
                last.compare(records[i].timestamp) <= 0);

            last = records[i].timestamp;
            id = records[i].id;
        }

        result.reset(nullptr);

        if (start == last) {
            // No new logs yet, wait a little.
            std::this_thread::sleep_for (std::chrono::seconds(1));
        }

        start = last;

    }
}

