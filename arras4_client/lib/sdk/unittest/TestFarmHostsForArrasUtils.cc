// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "TestFarmHostsForArrasUtils.h"
#include <sdk/FarmHostsForArrasUtils.h>
#include <message_api/Object.h>

#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <thread> 
#include <chrono>

namespace {
const char* CONFIG_BASE = "http://studio.svc.gld.dreamworks.net/pam/config/1/";
const char* DC = "gld";
const char* STACK = "prod";  
}

CPPUNIT_TEST_SUITE_REGISTRATION(TestFarmHostsForArrasUtils);

void
TestFarmHostsForArrasUtils::setUp()
{
    setenv("DWA_CONFIG_SERVICE", CONFIG_BASE, true);
    if (mFarmHostsForArrasUtils.get() == nullptr) {
        mFarmHostsForArrasUtils.reset(new arras4::sdk::FarmHostsForArrasUtils(DC, STACK));
    }
}

void
TestFarmHostsForArrasUtils::tearDown()
{
}

void
TestFarmHostsForArrasUtils::testGetBaseUrl()
{
    // To run this tests please make sure 
    // arras_node_launcher v1.0.18+ is running on the localhost 
#ifdef ENABLE_GET_FARM_HOSTS_TEST
    // Print debug stuff using std::cerr.  We don't want unittest debug to go to athena
    std::cerr<< "testGetBaseUrl\n";
    try{
        // should alway get this msh base url unless silo is not running
        // or multi_session_request_handler is not running under silo
        const std::string& msh_base_url = 
            arras4::sdk::FarmHostsForArrasUtils::getBaseUrl("gld");
        CPPUNIT_ASSERT(msh_base_url.empty() == false);
        std::cerr << "base url:" << msh_base_url << "\n";
    } catch (const std::exception &e) {
        std::string msg("Error TestFarmHostsForArrasUtils::testBaseUrl(): ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }
#else
    std::cerr << "Skip TestFarmHostsForArrasUtils::testGetBaseUrl()\n"
        << "Please run arras_node_launcher v1.0.18+ on the localhost."
        << "and set the following env before running unittest.\n"
        << " % setenv ENABLE_GET_FARM_HOSTS_TEST 1\n";
#endif
}


void
TestFarmHostsForArrasUtils::testPostRequest()
{
    // only do the post test when the env is set, and be ready to
    // kill the farm job when test is done.
#ifdef ENABLE_GET_FARM_HOSTS_TEST
    std::cerr << "Farm request post test:  Please make sure to kill the job when done.\n";

    const std::string
        &production = "trolls2",
        &user = std::string(getenv("USER"));
    int num_hosts = 2;
    try {
        int min_cores = 24;
        const std::string& max_cores = "*";
        const std::string& steering = "ih_hosts";
        const std::string& mem = "55";
        int prio = 1;
        int minutes = 189;
        const std::string& sl = "arras4_client unittest";
        int groupId = 
	    mFarmHostsForArrasUtils->postRequest(production, user, num_hosts, min_cores, max_cores, "", steering, prio, minutes, mem, sl);
        bool canCancel = false;
        for (int i = 0; i < 10; i++) {

            if (groupId <=0) break;  // no group

            // verify groupId in the group detail status
            std::this_thread::sleep_for (std::chrono::seconds(2));
            arras4::api::Object obj = mFarmHostsForArrasUtils->getDetail();
            std::cerr<<"obj ..." << obj << "\n";

            // should be no error, we just posted.
            bool hasError = obj.isMember("error");
            CPPUNIT_ASSERT(hasError == false);
            const std::vector<std::string>& groupIds = obj.getMemberNames();
            bool found = false;

            for (const auto& id : groupIds) {
                int gId = atoi(id.c_str());
                if (gId == groupId) {
                    std::cerr << "found groupId: " << groupId << " " << gId << "\n";
                    found = true;

                    // when job is in MRG/FC, then we can cancel it
                    canCancel = "WAIT" != obj[id]["status"].asString();
                    break;
                }
            }
            CPPUNIT_ASSERT(found);
            if (canCancel) {
                // group is pending, try set run limit
                if (groupId > 0) {
                    float hr = 2.4f;
                    std::cerr<<"set run-limit: " << groupId << " " << hr <<" hours\n";
                    bool ok = mFarmHostsForArrasUtils->setRunLimit(groupId, hr);
                    std::cerr<<"set run-limit " << groupId << "; ok:" << ok << "\n";
                    CPPUNIT_ASSERT(ok);
                }

                // stop the check-status loop and try cancel group next
                break;
            }
        }

        if (groupId > 0 && canCancel) {
            // cancel it.
            std::cerr<<"cancel group: " << groupId << "\n";
            bool ok = mFarmHostsForArrasUtils->cancelGroup(groupId);
            std::cerr<<"cancel group " << groupId << "; ok:" << ok << "\n";
            CPPUNIT_ASSERT(ok);
        }

    } catch (const std::exception &e) {
        std::string msg("Error TestFarmHostsForArrasUtils::testPostRequest(): ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }

#else
    std::cerr << "Skip TestFarmHostsForArrasUtils::testPostRequest()\n"
        << "To enable, do the following before running unittest.\n"
        << " % setenv ENABLE_GET_FARM_HOSTS_TEST 1\n";
    return;
#endif

}

void
TestFarmHostsForArrasUtils::testGetDetail()
{
    // To run this tests please make sure 
    // arras_node_launcher v1.0.18+ is running on the localhost 
#ifdef ENABLE_GET_FARM_HOSTS_TEST
    std::cerr<< "testGetDetail\n";
    const std::string 
        &status_key = arras4::sdk::FarmHostsForArrasUtils::STATUS_KEY,
        &num_key = arras4::sdk::FarmHostsForArrasUtils::NUM_KEY,
        &pend = arras4::sdk::FarmHostsForArrasUtils::PEND_STATUS,
        &wait = arras4::sdk::FarmHostsForArrasUtils::WAIT_STATUS,
        &run = arras4::sdk::FarmHostsForArrasUtils::RUN_STATUS;

    try{

        arras4::api::Object obj = mFarmHostsForArrasUtils->getDetail();
        std::cerr<< "testGetDetail: obj:" << obj <<"\n";
        // check if query return error: such as local server not running...
        if (obj.isMember("error")) {
            // if error, expecting an error message
            const std::string& msg = obj["error"].asString();
            std::cerr << "get detail query error message: " << msg << "\n";
            CPPUNIT_ASSERT(msg.empty() == false);
            return;
        }

        // expecting group status 
        const std::vector<std::string>& groupIds = obj.getMemberNames();
        for (const auto& groupId : groupIds) {

            arras4::api::Object& group = obj[groupId];
            std::cerr << "greoupId: " << groupId << "\n";
            const std::vector<std::string>& keys = group.getMemberNames();
            for (const auto& key : keys) {
                if (key == num_key) {
                    const int num = group[num_key].asInt();
                    std::cerr << "  " << key << ":" <<  num << "\n";
                    // expecting number of jobs/hosts to be greater than 0.
                    CPPUNIT_ASSERT(num > 0);
                } else if (key == status_key) {
                    const std::string& status = group[status_key].asString();
                    std::cerr << "  " << key << ":" <<  status << "\n";
                    // expecting status to be PEND, WAIT, or RUN
                    bool check_status = status == pend || status == wait || status == run;
                    CPPUNIT_ASSERT(check_status);
                } else {
                    CPPUNIT_FAIL("unexpected key in get detail query response");
                }
            }
        }

    } catch (const std::exception &e) {
        std::string msg("Error TestFarmHostsForArrasUtils::testGetDetail(): ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }
#else
    std::cerr << "Skip TestFarmHostsForArrasUtils::testGetDetail()\n"
        << "Please run arras_node_launcher v1.0.18+ on the localhost."
        << "and set the following env before running unittest.\n"
        << " % setenv ENABLE_GET_FARM_HOSTS_TEST 1\n";
#endif
}

void
TestFarmHostsForArrasUtils::testSetEnv()
{
    // To run this tests please make sure 
    // arras_node_launcher v1.0.18+ is running on the localhost 
#ifdef ENABLE_GET_FARM_HOSTS_TEST
    try{
        std::cerr<< "testSetEnv stb\n";
        bool ok = mFarmHostsForArrasUtils->setEnv("stb");
        CPPUNIT_ASSERT(ok);

        std::cerr<< "testSetEnv abc. Expecting failure.\n";
        ok = mFarmHostsForArrasUtils->setEnv("abc");
        CPPUNIT_ASSERT(ok == false);

        std::cerr<< "testSetEnv prod\n";
        ok = mFarmHostsForArrasUtils->setEnv("prod");
        CPPUNIT_ASSERT(ok);

    } catch (const std::exception &e) {
        std::string msg("Error TestFarmHostsForArrasUtils::testGetDetail(): ");
        msg += e.what();
        CPPUNIT_FAIL(msg);
    }
#endif
}


