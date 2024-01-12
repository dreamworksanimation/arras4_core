// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_TESTPEERCLASSES_H_
#define __ARRAS_TESTPEERCLASSES_H_

#include <cppunit/extensions/HelperMacros.h>

class TestPeerClasses: public CppUnit::TestFixture
{
public:
    TestPeerClasses()
        : CppUnit::TestFixture()
    {}

    void testSocketPeerConstructAndPoll();
    void testSocketPeerSend();
    void testSocketPeerPeek();
    void testSocketPeerReceive();
    void testSocketPeerAccept();
    void testIPCSocketPeer();
    void testInetSocketPeer();
    void testPeerClasses();

    CPPUNIT_TEST_SUITE(TestPeerClasses);
        CPPUNIT_TEST(testPeerClasses);
    CPPUNIT_TEST_SUITE_END();

};


#endif // __ARRAS_TESTPEERCLASSES_H_

