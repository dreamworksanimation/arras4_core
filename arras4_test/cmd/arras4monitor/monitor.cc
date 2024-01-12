// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <arras4_log/Logger.h>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <ctime>
#include <iostream>
#include <json/reader.h>
#include <time.h>

#include "Spreadsheet.h"
#include "Data.h"
#include "Print.h"

bool gFullDate = false;


namespace bpo = boost::program_options;

const std::string DEFAULT_ENV_NAME = "prod";

ColumnType
nameToColumnType(const std::string& name)
{
    if (name == "fid") {
        return FULL_ID;
    } else if (name == "id") {
        return SHORT_ID;
    } else if (name == "name") {
        return COMP_NAME;
    } else if (name == "compstat") {
        return COMP_STATUS;
    } else if (name == "node") {
        return NODE;
    } else if (name == "execstat") {
        return EXEC_STATUS;
    } else if (name == "reason") {
        return STOPPED_REASON;
    } else if (name == "sig") {
        return SIGNAL;
    } else if (name == "cpu") {
        return CPU_USAGE_5;
    } else if (name == "maxcpu") {
        return CPU_USAGE_5MAX;
    } else if (name == "cpu60") {
        return CPU_USAGE_60;
    } else if (name == "maxcpu60") {
        return CPU_USAGE_60MAX;
    } else if (name == "cputime") {
        return CPU_USAGE_TOTAL;
    } else if (name == "sent5") {
        return SENT_MESSAGES_5;
    } else if (name == "sent60") {
        return SENT_MESSAGES_60;
    } else if (name == "sent") {
        return SENT_MESSAGES_TOTAL;
    } else if (name == "lastsent") {
        return SENT_MESSAGE_TIME;
    } else if (name == "rcvd5") {
        return RECEIVED_MESSAGES_5;
    } else if (name == "rcvd60") {
        return RECEIVED_MESSAGES_60;
    } else if (name == "rcvd") {
        return RECEIVED_MESSAGES_TOTAL;
    } else if (name == "lastrcvd") {
        return RECEIVED_MESSAGE_TIME;
    } else if (name == "beat") {
        return HEARTBEAT_TIME;
    } else if (name == "mem") {
        return MEMORY;
    } else if (name == "rmem") {
        return RESERVED_MEMORY;
    } else if (name == "cores") {
        return RESERVED_CORES;
    } else if (name == "maxmem") {
        return MEMORY_MAX;
    } else if (name == "user") {
    // computation details
        return SESSION_CLIENT_USER;
    }

    fprintf(stderr,"arras4monitor: Unknown column type \"%s\"\n", name.c_str());
    exit(1);
}

const std::string
getStudioName()
{
    std::string studio = std::getenv("STUDIO");
    boost::to_lower(studio);
    return studio;
}

typedef std::vector<std::string> StringList;

void
parseCmdLine(int argc, char* argv[],
             bpo::options_description& flags,
             bpo::variables_map& cmdOpts)
{
    flags.add_options()
        ("help,h", "produce help message")
        ("env,e", bpo::value<std::string>()->default_value(DEFAULT_ENV_NAME),"Environment to connect to (e.g. 'prod' or 'uns')")
        ("dc", bpo::value<std::string>()->default_value(getStudioName()),"Datacenter to use (e.g. 'gld')")
        ("session,s", bpo::value<StringList>(), "Name(s) of Arras session to show (multiple instances of this option are allowed)")
        ("local","Use a local environment")
        ("user,u", bpo::value<std::string>()->default_value("")->implicit_value("self"), "Only show sessions created by the given user")
        ("format,f",bpo::value<StringList>(), "Names of fields to display (multiple instances of this option are allowed)")
        ("sort",bpo::value<StringList>(), "Names of fields to sort by in the sort order (multiple instances of this options are allowed)")
        ("brief,b","Don't show per computation details")
        ("logs,l",bpo::value<unsigned>()->default_value(0), "The number of log lines which will be shown per session")
        ("delay,d",bpo::value<float>()->default_value(5.0), "The number of seconds between refreshes (default 5)")
        ("n,n",bpo::value<unsigned>()->default_value(0), "The number of iterations of refresh (default indefinite)")
        ("wraplogs,w","Wrap long log lines (by default they are truncated)")
        ;

    bpo::store(bpo::command_line_parser(argc, argv).
               options(flags).run(), cmdOpts);
    bpo::notify(cmdOpts);
}


std::map<std::string,Node> sNodes;


namespace {

std::string
tolower (const std::string& str)
{
    std::string result;
    for (auto i = 0; i < str.length(); i++) {
        result += ::tolower(str[i]);
    }
    return result;
}

void
getList(const bpo::variables_map& opts, const std::string& optionName, std::vector<std::string>& list)
{
    int sessionCount = opts.count(optionName);
    if (sessionCount > 0) {
        StringList optionList = opts[optionName].as<StringList>();
        for (auto listIter= optionList.begin(); listIter!= optionList.end(); ++listIter) {
            std::string option = *listIter;
            auto index = 0;
            auto next = index;
            while ((next = option.find(',', index))  != std::string::npos) {
                list.push_back(tolower(option.substr(index,next-index)));
                index = next+1;
            }
            list.push_back(tolower(option.substr(index)));
        }
    }
}

void
getList(const std::string& option, std::vector<std::string>& list)
{
    auto index = 0;
    auto next = index;
    while ((next = option.find(',', index))  != std::string::npos) {
        list.push_back(tolower(option.substr(index,next-index)));
        index = next+1;
    }
    list.push_back(tolower(option.substr(index)));
}

} // end anonymous namespace

int
main(int argc, char* argv[])
{
    bpo::options_description flags;
    bpo::variables_map cmdOpts;
    std::map<std::string,Session> sessions;

    arras4::log::Logger::Level logLevel;
    arras4::log::Logger::instance().setThreshold(arras4::log::Logger::LOG_FATAL);

    try {
        parseCmdLine(argc, argv, flags, cmdOpts);
    } catch(std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    } catch(...) {
        std::cerr << "Exception of unknown type!" << std::endl;;
        return 1;
    }

    if (cmdOpts.count("help")) {
        std::cout << flags << std::endl;
        return 0;
    }

    //
    // process the list of sessions to show
    //
    std::vector<std::string> sessionFilter;
    getList(cmdOpts, "session", sessionFilter);

    //
    // decide what format the spreadsheet should be
    //
    std::vector<std::string> fieldList;
    getList(cmdOpts, "format", fieldList);
    if (fieldList.size() == 0) {
        getList("id,user,name,node,cpu,maxcpu,cputime,lastsent,lastrcvd,beat", fieldList);
    }
    std::vector<ColumnType> columns;
    for (auto i = 0; i < fieldList.size(); i++) {
        ColumnType type = nameToColumnType(fieldList[i]);
        if (type != COLUMNTYPE_INVALID) columns.push_back(type);
    }

    // 
    // decide the sorting of the spreadsheet
    //
    std::vector<std::string> sortList;
    getList(cmdOpts, "sort", sortList);
    if (sortList.size() == 0) {
        getList("~cpu", sortList);
    }
    std::vector<ColumnType> sortOrder;
    std::vector<bool> sortDirection;
    for (auto i = 0; i < sortList.size(); i++) {
        std::string sortName = sortList[i];
        bool reverse;
        if (sortName[0] == '~') {
            reverse = true;
            sortName = sortName.substr(1);
        } else {
            reverse = false;
        }
        ColumnType type = nameToColumnType(sortName);
        if (type != COLUMNTYPE_INVALID) {
            sortOrder.push_back(type);
            sortDirection.push_back(reverse);
        }
    }

    // GLD or LAS
    std::string datacenter = cmdOpts["dc"].as<std::string>();
    // UNS, STD, STB2, PROD, etc
    std::string environment = cmdOpts["env"].as<std::string>();

    bool uselocal = (cmdOpts.count("local") > 0);

    std::string coordinator;
    std::string logService;
    std::string consul;

    if (uselocal) {
        coordinator = "http://localhost:8087/coordinator/1";
        // logs = 
        // consul = 
    } else {
        if (initServiceUrls(datacenter, environment, coordinator, logService, consul)) {
            fprintf(stderr,"Couldn't get environment config\n");
            exit(1);
        }
    }

/*
    printf("coordinator=%s\n", coordinator.c_str());
    printf("logService=%s\n", logService.c_str());
    printf("consul=%s\n", consul.c_str());
*/

    std::string user;
    if (cmdOpts.count("user")) {
        user = cmdOpts["user"].as<std::string>();
        if (user == "self") {
            char* userPtr = getenv("USER");
            if (userPtr != NULL) user = std::string(userPtr);
        }
    } else {
        user = "";
    }

    bool wrapLogs = (cmdOpts.count("wraplogs") != 0);
    bool detailed = (cmdOpts.count("brief") == 0);
    int logs = cmdOpts["logs"].as<unsigned>();
    float delay = cmdOpts["delay"].as<float>();
    unsigned long microseconds = delay * 1000000;
    if (delay <= 0.0) {
        fprintf(stderr,"arras4monitor: invalid delay (%g). It must be a positive number\n",delay);
        exit(1);
    }
    unsigned int iterations = cmdOpts["n"].as<unsigned>();

    std::vector<Session> sortedSessions;
    while (1) {
        getSessions(coordinator, user, sessionFilter, sNodes, sessions);
        getComputationDetails(sNodes, sessions);
        aggregateComputationStats(sessions);
        SortSessions(sessions, sNodes, sortedSessions, sortOrder, sortDirection);
        if (logs > 0) getLogs(logService, sortedSessions, logs);
        Spreadsheet* spreadsheet = CreateSpreadsheet(sortedSessions, sNodes, columns, detailed, gFullDate, (logs > 0));
        spreadsheet->print(wrapLogs);
        delete spreadsheet;

        // 0 is iterate forever, 2+ has more to go
        if (iterations==1) break;

        usleep(microseconds);

        if (iterations > 0) iterations--;
    }
    printf("\n");
    return 0;
}

