// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_TEST_FARMHOSTSFORARRASUTILS_H_
#define __ARRAS4_TEST_FARMHOSTSFORARRASUTILS_H_

#include <memory>
#include <sstream>

#include <cppunit/extensions/HelperMacros.h>

#include <sdk/FarmHostsForArrasUtils.h>


class TestFarmHostsForArrasUtils: public CppUnit::TestFixture
{
public:
    TestFarmHostsForArrasUtils()
        : CppUnit::TestFixture()
    {}

    void setUp();
    void tearDown();
    void testGetBaseUrl();
    void testPostRequest(); // tests post, set runlimit, and cancel
    void testGetDetail();
    void testSetEnv();

    CPPUNIT_TEST_SUITE(TestFarmHostsForArrasUtils);
        CPPUNIT_TEST(testGetBaseUrl);
        CPPUNIT_TEST(testPostRequest);
        CPPUNIT_TEST(testGetDetail);
        CPPUNIT_TEST(testSetEnv);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<arras4::sdk::FarmHostsForArrasUtils> mFarmHostsForArrasUtils;
};

#endif // __ARRAS4_TEST_FARMHOSTSFORARRASUTILS_H_

