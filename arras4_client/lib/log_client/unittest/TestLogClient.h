// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_TESTLOGCLIENT_H_
#define __ARRAS4_TESTLOGCLIENT_H_

#include <memory>
#include <sstream>

#include <cppunit/extensions/HelperMacros.h>

#include <log_client/LogClient.h>


class TestLogClient: public CppUnit::TestFixture
{
public:
    TestLogClient()
        : CppUnit::TestFixture()
        , logClient(nullptr)
    {}

    void setUp();
    void tearDown();
    void testGetByHost();
    void testGetByHostDesc();
    void testGetByHostWithPage();
    void testGetByHostWithDate();
    void testGetByHostWithPageAndDate();
    void testGetBySessionId();
    void testGetBySessionIdAndCompName();
    void testGetBySessionIdAndTime();
    void testGetBySessionIdAndTimeEmptyEndTime();
    void testGetBySessionIdComplete();
    void testTail();

    CPPUNIT_TEST_SUITE(TestLogClient);
        CPPUNIT_TEST(testGetByHost);
        CPPUNIT_TEST(testGetByHostDesc);
        CPPUNIT_TEST(testGetByHostWithPage);
        CPPUNIT_TEST(testGetByHostWithDate);
        CPPUNIT_TEST(testGetByHostWithPageAndDate);
        CPPUNIT_TEST(testGetBySessionId);
        CPPUNIT_TEST(testGetBySessionIdAndCompName);
        CPPUNIT_TEST(testGetBySessionIdAndTime);
        CPPUNIT_TEST(testGetBySessionIdAndTimeEmptyEndTime);
        CPPUNIT_TEST(testGetBySessionIdComplete);
        CPPUNIT_TEST(testTail);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<arras4::log_client::LogClient> logClient;
    std::ostringstream url;
    std::string sessionId;
    std::string sessionStart;
    std::string sessionEnd;
    std::string sessionNode;
    std::string sessionCompName;
};

#endif // __ARRAS4_TESTCLIENT_H_

