// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ExecComp.h"

#include <message_api/Object.h>
#include <shared_impl/ExecutionLimits.h>
#include <shared_impl/ProcessExitCodes.h>
#include <arras4_athena/AthenaLogger.h>
#include <arras4_log/AutoLogger.h>
#include <arras4_log/LogEventStream.h>

#if not defined(DONT_USE_CRASH_REPORTER)
#include <arras4_crash/CrashReporter.h>
#endif

#include <boost/program_options.hpp>

#include <fstream>
#include <sstream>
#include <iostream>

#include <linux/oom.h> // OOM_SCORE_ADJ_MAX

// parses out the command line options for the execComp command,
// then runs an instance of ExecComp to do the actual work

using namespace arras4::api;
using namespace arras4::impl;
using namespace arras4::log;

namespace {

unsigned short constexpr SYSLOG_PORT = 514;

namespace bpo = boost::program_options;

void adjustOomScore()
{
    // we set the "out of memory" score as high as possible so that,
    // if memory runs out, Linux kills the executor(s) in preference
    // to the node itself. TODO : verify that this based on valid assumptions
    try {
        std::ofstream oomFile("/proc/self/oom_score_adj");
        oomFile << OOM_SCORE_ADJ_MAX << std::endl;
    } catch (const std::exception& e) {
        ARRAS_ERROR(Id("oomFailure") <<
                    "Unable to adjust out of memory (oom) process settings, due to a write exception: " <<
                    std::string(e.what()));
    }
}

std::string envConfigMapper(const std::string& envVar) {
    std::string mappedKey;
    if (envVar == "ARRAS_ATHENA_ENV") {
        mappedKey = "athena-env";
    } else if (envVar == "ARRAS_ATHENA_HOST") {
        mappedKey = "athena-host";
    } else if (envVar == "ARRAS_ATHENA_PORT") {
        mappedKey = "athena-port";
    }

    return mappedKey;
}

void
parseCmdLine(int argc, char* argv[],
             bpo::options_description& flags, bpo::variables_map& cmdOpts)
{
    flags.add_options()
        ("memoryMB", bpo::value<size_t>()->default_value(DEFAULT_MEM_MB), "Memory (MB) allocated to this computation")
        ("use_affinity", bpo::value<bool>()->default_value(true), "Enable CPU affinity (set --use_affinity=0 to disable)")
        ("use_color", bpo::value<bool>()->default_value(true), "Enable ANSI color output in logs (set --use-color=0 to disable)")
        ("cores", bpo::value<float>()->default_value(1), "Number of cores to use")
        ("threadsPerCore", bpo::value<unsigned>()->default_value(1), "Number of hyperthreads per core")
        ("processorList", bpo::value<std::string>(), "List of processors to use when affinity is enabled")
        ("hyperthreadProcessorList", bpo::value<std::string>(), "List of hyperthreads to use when affinity is enabled")
        ("configFile", bpo::value<std::string>(), "Path to config file")

        // included for compatibility, but ignored
        ("disableChunking", bpo::value<bool>()->default_value(false),"Disable message chunking")
        ("minimumChunkingSize", bpo::value<std::size_t>()->default_value(0),"Minimum size of message (in bytes) to begin chunking (0=default)")
        ("chunkSize", bpo::value<std::size_t>()->default_value(0),"Size of each chunk (in bytes, 0=default)")

    ;

    bpo::options_description envOpts("Env Options");
    envOpts.add_options()
        ("athena-env", bpo::value<std::string>()->default_value("prod"))
        ("athena-host", bpo::value<std::string>()->default_value("localhost"))
        ("athena-port", bpo::value<unsigned short>()->default_value(SYSLOG_PORT));

    bpo::positional_options_description positionals;
    positionals.add("configFile", 1);

    bpo::store(bpo::parse_environment(envOpts, envConfigMapper), cmdOpts);
    bpo::store(bpo::command_line_parser(argc, argv).
               options(flags).positional(positionals).run(), cmdOpts);
    bpo::notify(cmdOpts);
}

bool initializeLimits(ExecutionLimits& limits, bpo::variables_map& cmdOpts)
{
    limits.setUnlimited(false);
    size_t memoryMB = cmdOpts["memoryMB"].as<size_t>();
    if (memoryMB > std::numeric_limits<unsigned int>::max()) {
        ARRAS_ERROR(Id("memReqTooLarge") << "Memory request too large");
        return false;
    }
    limits.setMaxMemoryMB((unsigned)memoryMB);
    float maxCores = cmdOpts["cores"].as<float>();
    if (maxCores < 0) {
        ARRAS_ERROR(Id("badCoresValue") << "cores value must the 0.0 or greater");
        return false;
    }
    limits.setMaxCores((unsigned)maxCores);
    limits.setThreadsPerCore(cmdOpts["threadsPerCore"].as<unsigned>());
    bool affinity = cmdOpts["use_affinity"].as<bool>();
    if (affinity) {
         std::string cpuSet;
         if (cmdOpts.count("processorList"))
             cpuSet = cmdOpts["processorList"].as<std::string>();
         else {
             ARRAS_ERROR(Id("missingProcList") 
                         << "You must specific a processor list if affinity is not disabled");
             return false;
         }
         std::string htCpuSet;
         if (cmdOpts.count("hyperthreadProcessorList")) {
             htCpuSet = cmdOpts["hyperthreadProcessorList"].as<std::string>();
         } else if (limits.usesHyperthreads()) {
             ARRAS_ERROR(Id("missingHyperProcList") <<
                         "You must specific a hyperthread processor list if affinity is not disabled and you have specified more than one thread per core");
             return false;
         }
         limits.enableAffinity(cpuSet,htCpuSet);
    }
    return true;
}

bool loadConfig(ObjectRef config, bpo::variables_map& cmdOpts)
{
    if (cmdOpts.count("configFile") == 0) {
        ARRAS_ERROR(Id("missingConfig") << "No config file was provided");
        return false;
    }
    std::string path = cmdOpts["configFile"].as<std::string>();
    std::ifstream ifs(path);
    if (ifs.fail()) {
        ARRAS_ERROR(Id("configFileError") << "Failed to open config file '" << path << "'");
        return false;
    }
    try {
        ifs >> config;
        return true;
    } catch (std::exception& e) {
        ARRAS_ERROR(Id("configFileError") <<
                    "Error reading config file '" << path
                    << "': " << e.what());
        return false;
    }
}

} // end anonymous namespace

int
main(int argc, char* argv[])
{
    // parse the command line arguments
    bpo::options_description flags;
    bpo::variables_map cmdOpts;
    try {
        parseCmdLine(argc, argv, flags, cmdOpts);
    } catch (std::exception& e) {
        std::cerr << "execComp error parsing command line : " << e.what() << std::endl;
        return ProcessExitCodes::INVALID_CMDLINE;
    } catch(...) {
        std::cerr << "execComp error parsing command line : (unknown exception)" << std::endl;
        return ProcessExitCodes::INVALID_CMDLINE;
    }

    // set the default logger to be an AthenaLogger
    AthenaLogger& athenaLogger = AthenaLogger::createDefault("comp",
                                cmdOpts["use_color"].as<bool>(),
                                cmdOpts["athena-env"].as<std::string>(),
                                cmdOpts["athena-host"].as<std::string>(),
                                cmdOpts["athena-port"].as<unsigned short>());

    // causes stdout and stderr to go to default logger
    AutoLogger autoLogger;

    // the configured log level is not set until the start of ExecComp.run(),
    // so if you want debug logging to happen inside this code, set the
    // threshold here:
    // Logger::instance().setThreshold(arras::log::Logger::LOG_DEBUG);
    athenaLogger.setThreadName("main");

    // adjust the "Out of Memory" killer settings for this process
    adjustOomScore();

    ExecutionLimits limits;
    if (!initializeLimits(limits,cmdOpts))
        return ProcessExitCodes::INVALID_CMDLINE;

    Object config;
    if (!loadConfig(config,cmdOpts))
        return ProcessExitCodes::CONFIG_FILE_LOAD_ERROR;

    const std::string& sessionId = config["sessionId"].asString();
    athenaLogger.setSessionId(sessionId);

#if not defined(DONT_USE_CRASH_REPORTER)
    // breakpad for this process:
    std::string breakpadPath;
    const char *tmp = getenv("ARRAS_BREAKPAD_PATH");
    if (tmp != nullptr) breakpadPath = tmp;
    const std::string& compName = config["config"].begin().memberName();
    const std::string& dsoName = (*config["config"].begin())["dso"].asString();
    INSTANCE_CRASH_REPORTER_WITH_INSTALL_PATH(argv[0], breakpadPath, sessionId, compName, dsoName);
#endif

    ExecComp execComp(limits,config,athenaLogger);
    int ret = execComp.run();

    return ret;
}


