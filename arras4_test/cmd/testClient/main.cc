// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Data.h"

#include <atomic>
#include <arras4_log/Logger.h>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <computation_api/Computation.h>
#include <client/api/SessionDefinition.h>
#include <dlfcn.h>
#include <fstream>
#include <json/json.h>
#include <message/TestMessage.h>
#include <message_api/Message.h>
#include <sdk/sdk.h>
#include <thread>

#if defined(JSONCPP_VERSION_MAJOR)
#define memberName name
#endif

namespace bpo = boost::program_options;
using namespace arras4::test;

unsigned short constexpr DEFAULT_COORDINATOR_PORT = 8087;
const std::string DEFAULT_COORDINATOR_PATH = "/coordinator/1/sessions";
const std::string DEFAULT_ENV_NAME = "prod";
const std::string DEFAULT_SESSION_NAME = "simple_test";
unsigned short constexpr MAX_WAIT_FOR_READY_SECS = 30;
unsigned short constexpr MAX_WAIT_FOR_DISCONNECT_SECS = 20;
unsigned constexpr DEFAULT_STAY_CONNECTED_SECS = 10;
unsigned short constexpr DEFAULT_LOG_LEVEL = 3;
int constexpr ERROR_EXIT_CODE = 1;

std::atomic<bool> arrasUnexpectedStop(false);
std::atomic<bool> arrasExceptionThrown(false);

std::string sessionIdStr;

//
// check that every member in aSubset is in aSuperset and the same value
// Arrays are expected to be the same size but the order of members can
// can be different.
//
bool
subset(const Json::Value& aSubset, const Json::Value& aSuperset,
       const std::string& aVarName, bool aPrintError)
{

    if (aSuperset.isNull()) {
        if (aPrintError) {
            ARRAS_LOG_ERROR("%s doesn't exist in superset", aVarName.c_str());
        }
        return false;
    }
    if (aSuperset.type() != aSubset.type()) {
        if (aPrintError) {
            ARRAS_LOG_ERROR("Types don't match on %s", aVarName.c_str());
        }
        return false;
    }
    switch (aSubset.type()) {
      case Json::nullValue:
      case Json::intValue:
      case Json::uintValue:
      case Json::realValue:
      case Json::stringValue:
      case Json::booleanValue:
        {
            bool matches = (aSubset == aSuperset);
            if (!matches) {
                if (aPrintError) {
                    ARRAS_LOG_ERROR("%s doesn't match (expected\n%s\nfound\n%s\n)\n",
                                    aVarName.c_str(), aSubset.asString().c_str(), aSuperset.asString().c_str());
                }
            }
            return (aSubset == aSuperset);
        }
    
      case Json::arrayValue:
        {
            unsigned int size = aSubset.size();
            if (size != aSuperset.size()) {
                if (aPrintError) {
                    ARRAS_LOG_ERROR("The array sizes of %s don't match (expected %d, found %d)",
                                     aVarName.c_str(), size, aSuperset.size());
                    return false;
                }
            }
            for (auto i = 0; i < size; i++) {

                // find a match in the array (assuming order doesn't matter)
                unsigned int j;
                for (j = 0; j < size; j++) {
                    std::string name = aVarName + "[" +
                                       std::to_string(i) + " vs " + std::to_string(j) +
                                       "]";
                    bool matches = subset(aSubset[i], aSuperset[j], name, false);
                    if (matches) break;
                }
                if (j == size) {
                    // if there was no match and error printing is on then print
                    // why each one failed
                    ARRAS_LOG_ERROR("%s[%d] had no matches", aVarName.c_str(), i);
                    if (aPrintError) {
                        for (j = 0; j < size; j++) {
                            std::string name = aVarName + "[" +
                                               std::to_string(i) + " vs " + std::to_string(j) +
                                               "]";
                            subset(aSubset[i], aSuperset[j], name, true);
                        }
                        return false;
                    }
                }
            }
            return true;
        }
        break;
      case Json::objectValue:
        {
            unsigned int index = 0;
            Json::ValueConstIterator iter = aSubset.begin();
            while (iter != aSubset.end()) {
                std::string member_name(iter.memberName());
                std::string full_name = aVarName + "." + iter.memberName();
                Json::Value value = aSuperset[member_name.c_str()];
                if (value.isNull()) {
                    if (aPrintError) {
                        ARRAS_LOG_ERROR("Superset is missing %s", full_name.c_str());
                        return false;
                    }
                }
                if (!subset(*iter, value, full_name, aPrintError)) return false;
                ++iter;
            }
            return true;
        }
        break;

      default:
        ARRAS_LOG_ERROR("%s is unknown Json type", aVarName.c_str());
        return false;
    }
    return false;
}

bool
subset(const std::string& aSubsetStr, const std::string& aSupersetStr)
{
    Json::Value subsetJson, supersetJson;

    Json::Reader reader;
    reader.parse(aSubsetStr, subsetJson);
    reader.parse(aSupersetStr, supersetJson);

    return subset(subsetJson, supersetJson, "toplevel", true);
}

const std::string getStudioName()
{
    char *pStudio = std::getenv("STUDIO");
    if (pStudio) {
        std::string studio = std::getenv("STUDIO");
        boost::to_lower(studio);
        return studio;
    }
    return "gld";
}

void parseCmdLine(int argc, char* argv[],
                  bpo::options_description& flags, 
                  bpo::variables_map& cmdOpts) 
{
    
    flags.add_options()
        ("help", "produce help message")
        ("env", bpo::value<std::string>()->default_value(DEFAULT_ENV_NAME),"Environment to connect to (e.g. 'prod' or 'uns')")
        ("dc", bpo::value<std::string>()->default_value(getStudioName()),"Datacenter to use (e.g. 'gld')")
        ("host", bpo::value<std::string>(), "Host to connect to (in place of env/dc : for example, use 'localhost' with arras.sh")
        ("port", bpo::value<unsigned short>()->default_value(DEFAULT_COORDINATOR_PORT), "Port to use (with '--host')")
        ("session,s", bpo::value<std::string>()->default_value(DEFAULT_SESSION_NAME), "Name of Arras session to use")
        ("log-level,l", bpo::value<unsigned short>()->default_value(DEFAULT_LOG_LEVEL), "Log level [0-5] with 5 being the highest")
        ("stay-connected",bpo::value<unsigned>()->default_value(DEFAULT_STAY_CONNECTED_SECS), "Number of seconds to remain connected after all messages have been sent")
        ("bytes",bpo::value<unsigned>()->default_value(0), "Random data block size (bytes)")
        ("mb",bpo::value<unsigned>()->default_value(0),  "Random data block size (megabytes)")
        ("count,c",bpo::value<unsigned>()->default_value(1), "Number of messages to send")
        ("interval,i",bpo::value<unsigned>()->default_value(1), "Interval between message sends (seconds)")
        ("forceError", bpo::value<std::vector<std::string>>()->multitoken()->composing(), "Errors to force : use [DEFER] (THROW|SEGFAULT|CORRUPT) (IN_SERIALIZE|IN_DESERIALIZE)")
        ("deferError",bpo::value<unsigned>()->default_value(0),"Defers error until message has been serialized N times")
        ("sleepInConfig",bpo::value<unsigned>(),"Computation sleeps in config for N seconds")
        ("minChunkingMb",bpo::value<unsigned>(),"Minimum message size for chunking (megabytes)")
        ("minChunkingBytes",bpo::value<unsigned>(),"Minimum message size for chunking (bytes)")
        ("chunkSizeMb",bpo::value<unsigned>(),"Chunk size (megabytes)")
        ("chunkSizeBytes",bpo::value<unsigned>(),"Chunk size (bytes)")
        ("disconnectImmediately","Test that disconnects immediately after connecting")
        ("prepend", bpo::value<std::string>(), "Path to prepend to packages path")
        ("local-only", "Only run on a local node")
        ("expected-status",bpo::value<std::string>()->default_value(std::string()), "The expect json exit status of the session")
        ("expect-disconnect","Indicate that the sessions is expected to shutdown before requested to")
        ("expect-connect-error","Indicate that the sessions is expected to fail the connection")
        ("negotiate-error", bpo::value<unsigned>(),"Error type negotiating node connection")
        ("get-logs", "Print the node logs for the session")
        ("trace-level",bpo::value<int>()->default_value(0),"trace threshold level (-1=none,5=max)")
        ("save-incoming",bpo::value<std::string>(), "Path to save testcomp incoming messages")
        ("save-outgoing",bpo::value<std::string>(), "Path to save testcomp outgoing messages")
        ("save-config",bpo::value<std::string>(), "Path to save testcomp configuration")
        ("client-save-incoming",bpo::value<std::string>(), "Path to save client incoming messages")
        ("client-save-outgoing",bpo::value<std::string>(), "Path to save client outgoing messages")
        ("save-definition",bpo::value<std::string>(), "Path to save session definition")
        ("large-log",bpo::value<unsigned>()->default_value(0), "Size of a large log message to send")

        ;
    bpo::store(bpo::command_line_parser(argc, argv).
               options(flags).run(), cmdOpts);
    bpo::notify(cmdOpts);
}

std::string getCoordinatorUrl(arras4::sdk::SDK& sdk, const bpo::variables_map& cmdOpts)
{
    std::string url;
    std::string cmd_dc = cmdOpts["dc"].as<std::string>();
    std::string cmd_env = cmdOpts["env"].as<std::string>();
    if (cmdOpts.count("host")) {
        std::ostringstream ss;
        ss << "http://" << cmdOpts["host"].as<std::string>()
           << ":" << cmdOpts["port"].as<unsigned short>()
           << DEFAULT_COORDINATOR_PATH;

        url = ss.str();
    } else if (cmd_env=="local") {
        std::ostringstream ss;
        ss << "http://localhost:8087" 
           << DEFAULT_COORDINATOR_PATH;
        url = ss.str();
    } else {
        url = sdk.requestArrasUrl(cmd_dc, cmd_env);
        std::ostringstream msg;
        msg << "Received " << url << " from Studio Config Service.";
        ARRAS_LOG_INFO(msg.str());
    }

    return url;
}

bool connect(arras4::sdk::SDK& sdk,
             const std::string& sessionName,
             const bpo::variables_map& cmdOpts)
{
    try {
        fprintf(stderr,"Load session %s\n", sessionName.c_str());
        arras4::client::SessionDefinition def = arras4::client::SessionDefinition::load(sessionName);
        arras4::api::ObjectConstRef computations = def.getObject()["computations"];
        if (computations.isMember("testcomp")) {
            if (cmdOpts.count("sleepInConfig")) {
                def["testcomp"]["sleepInConfig"] = cmdOpts["sleepInConfig"].as<unsigned>();
            }
            if (cmdOpts.count("prepend")) {
                def["testcomp"]["requirements"]["rez_packages_prepend"] = cmdOpts["prepend"].as<std::string>();
            }
            if (cmdOpts.count("local-only")) {
                def["testcomp"]["requirements"]["local_only"] = std::string("yes");
            }
            if (cmdOpts.count("trace-level")) {
                def["testcomp"]["traceThreshold"] = cmdOpts["trace-level"].as<int>();
            }
        }
        if (cmdOpts.count("save-incoming")) {
            def["testcomp"]["saveIncomingTo"] = cmdOpts["save-incoming"].as<std::string>();
        }
        if (cmdOpts.count("save-outgoing")) {
            def["testcomp"]["saveOutgoingTo"] = cmdOpts["save-outgoing"].as<std::string>();
        }
        if (cmdOpts.count("save-config")) {
            def["testcomp"]["saveConfigTo"] = cmdOpts["save-config"].as<std::string>();
        }
        if (cmdOpts.count("client-save-incoming")) {
            def["(client)"]["saveIncomingTo"] = cmdOpts["client-save-incoming"].as<std::string>();
        }
        if (cmdOpts.count("client-save-outgoing")) {
            def["(client)"]["saveOutgoingTo"] = cmdOpts["client-save-outgoing"].as<std::string>();
        }
        if (cmdOpts.count("save-definition")) {
            def["(client)"]["saveDefinitionTo"] = cmdOpts["save-definition"].as<std::string>();
        }

        arras4::client::SessionOptions so;
        std::string coordinatorUrl = getCoordinatorUrl(sdk, cmdOpts);
        sdk.createSession(def, coordinatorUrl, so);

        ARRAS_LOG_INFO("Created session with ID %s",sdk.sessionId().c_str());
        sessionIdStr= sdk.sessionId();

        if (cmdOpts.count("disconnectImmediately")) {
            ARRAS_LOG_WARN("--disconnectImmediately specified : disconnecting now for testing");
            sdk.disconnect();
        }

    } catch (arras4::sdk::SDKException& e) {
        ARRAS_LOG_ERROR("Unable to connect to Arras: %s", e.what());
        return false;
    } catch (arras4::client::DefinitionLoadError& e) {
        ARRAS_LOG_ERROR("Failed to load session: %s", e.what());
        return false;
    }
    return true;
}

void messageHandler(const arras4::api::Message& message)
{
    if (message.classId() == TestMessage::ID) {
        TestMessage::ConstPtr tm = message.contentAs<TestMessage>();
        std::string source = message.get("sourceId").asString();
        ARRAS_LOG_INFO("Received: %s (Source %s)",tm->describe().c_str(),source.c_str());
    } else {
        ARRAS_LOG_INFO("Received: %s",message.describe().c_str());
    }
}

std::string shutdownStatus;

void statusHandler(const std::string& status)
{
    shutdownStatus = status;

    // Check to see if the new status is a json doc
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(status, root)) {
        Json::Value execStatus = root.get("execStatus", Json::Value());
        if (execStatus.isString() && execStatus.asString() == "stopped") {
        
            Json::Value stopReason = root.get("execStoppedReason", Json::Value());

            std::ostringstream msg;
            msg << "Status Handler : the Arras session has stopped";

            if (stopReason.isString()) {
                if (stopReason.asString() == "clientShutdown") {
                    msg << " as requested by client shutdown";
                    ARRAS_LOG_INFO(msg.str());
                    return;
                } else {    
                   
                    msg << " due to: " << stopReason.asString();
                }
            }

            arrasUnexpectedStop = true;
            ARRAS_LOG_WARN(msg.str());
            ARRAS_LOG_WARN("Computation Status:");

            Json::Value computations = root.get("computations", Json::Value());
            if (computations.isArray()) {
                for (const auto& comp : computations) {
                    Json::Value compName = comp.get("name", Json::Value());
                    Json::Value compStopReason = comp.get("stoppedReason", Json::Value());
                    Json::Value compSignal = comp.get("signal", Json::Value());

                    if (compName.isString() && compStopReason.isString()) {
                        msg.str(std::string());
                        msg << "\t" << compName.asString() << " stopped due to: " << compStopReason.asString();

                        // when the computation is stopped by a signal (terminate or kill)
                        // then compStopReason == "signal"
                        if (compSignal.isString() && compSignal.asString() != "not set") {
                            msg << " " << compSignal.asString();
                        }

                        ARRAS_LOG_WARN(msg.str());
                    }

                }
            }
        }
    } else {
        ARRAS_LOG_INFO("Status Handler : received status change to: %s", status.c_str());
    }
}

void exceptionCallback(const std::exception& e)
{
    ARRAS_LOG_ERROR("Thrown exception: %s", e.what());
    arrasExceptionThrown = true;
}

void sendTestMessage(arras4::sdk::SDK& sdk,unsigned index,unsigned count,size_t dataSize,
    unsigned forcedErrors) 
{
    TestMessage::Ptr tm = std::make_shared<TestMessage>(index,
                                                        "from client",
                                                        dataSize);
    tm->forcedErrors() = forcedErrors;
    arras4::api::UUID srcId = arras4::api::UUID::generate();
    std::string source = srcId.toString();
    sdk.sendMessage(tm,  arras4::api::withSource(source));
    ARRAS_LOG_INFO("Sent %d of %d : %s (Source %s)",index,count,tm->describe().c_str(),source.c_str()); 

}

unsigned parseForcedErrors(const bpo::variables_map& cmdOpts)
{
    if (cmdOpts.count("forceError") == 0)
        return 0;
    
    unsigned fe = 0;
    for (const std::string& s : cmdOpts["forceError"].as<std::vector<std::string>>()) {
        if (s == "THROW") fe |= THROW;
        else if (s == "SEGFAULT") fe |= SEGFAULT;
        else if (s == "CORRUPT") fe |= CORRUPT;
        else if (s == "IN_SERIALIZE") fe |= IN_SERIALIZE;
        else if (s == "IN_DESERIALIZE") fe |= IN_DESERIALIZE;
        else {
            ARRAS_LOG_ERROR("Unknown option '%s' in --forceError",s.c_str());
            return 0;
        }
    }
    if (fe == 0) return 0;
    
    unsigned defer = cmdOpts["deferError"].as<unsigned>();
    if (defer > DEFERMASK) {
        ARRAS_LOG_ERROR("value %d is too large for --deferError (maximum is %d)",defer,DEFERMASK);
        return 0;
    }
    fe |= defer;

    std::string message;
    unsigned et = fe & ERRTYPEMASK;
    if (et == THROW) message = "Will throw an exception ";
    else if (et == SEGFAULT) message = "Will cause a segfault ";
    else if (et == CORRUPT) message = "Will corrupt the message stream ";
    else {
        ARRAS_LOG_ERROR("--forceError must specify THROW, SEGFAULT or CORRUPT");
        return 0;
    }
    if (fe & IN_SERIALIZE) message += "in message 'serialize'";
    else if (fe & IN_DESERIALIZE) message += "in message 'deserialize'";
    else {
        ARRAS_LOG_ERROR("--forceError must specify IN_SERIALIZE or IN_DESERIALIZE");
        return 0;
    }
    if (defer > 0) message += " after " + std::to_string(defer) + " serializations.";
    else message += ".";
    ARRAS_LOG_INFO(message);
    return fe;
}

bool runSession(arras4::sdk::SDK& sdk,  
                const bpo::variables_map& cmdOpts)
{
    std::string sessionName = cmdOpts["session"].as<std::string>();
    unsigned stayConnectedSecs = cmdOpts["stay-connected"].as<unsigned>();
    size_t dataSize = 0;
    dataSize += cmdOpts["mb"].as<unsigned>() * 1024 * (size_t)1024;
    dataSize += cmdOpts["bytes"].as<unsigned>();
    unsigned count = cmdOpts["count"].as<unsigned>();
    unsigned interval = cmdOpts["interval"].as<unsigned>();
    bool expectDisconnect = cmdOpts.count("expect-disconnect");

    unsigned forcedErrors = parseForcedErrors(cmdOpts);

    if (!connect(sdk, sessionName, cmdOpts)) {
        ARRAS_LOG_ERROR("Exiting due to failed connection");
        return false;
    }

    // this can fail due to:
    //     unable to load the computation
    //         the file doesn't exist
    //         there are undefined symbols
    //         there are missing libraries it depends on
    //     computation doesn't finish initializing in a timely manner
    //     computation crashes during initialization
    bool ready = sdk.waitForEngineReady(MAX_WAIT_FOR_READY_SECS);
    if (!ready) {
        ARRAS_LOG_ERROR("Exiting : timed out waiting for engine to be ready");
        return false;
    }

    ARRAS_LOG_INFO("Client connected OK");
 
    for (unsigned index = 1; index <= count; index++) {
        if (index > 1 && interval > 0)
            sleep(interval);
        try {
            sendTestMessage(sdk,index,count,dataSize,forcedErrors);
        } catch (const arras4::client::ClientException& e) {
            if (expectDisconnect) {
                ARRAS_LOG_INFO("Got an exception sending a test message as expected: %s", e.what());
                return true;
            } else {
                ARRAS_LOG_ERROR("Got an unexpected exception sending a message: %s", e.what());
                return false;
            }
        }
    }

    if (stayConnectedSecs > 0) {
        ARRAS_LOG_INFO("Last message sent : staying connected for %d seconds",stayConnectedSecs);
        sleep(stayConnectedSecs);
    }
    return true;
}

void
initChunking(arras4::sdk::SDK& sdk,  
             const bpo::variables_map& cmdOpts)
{
    size_t minChunkingSize = 0;
    if (cmdOpts.count("minChunkingMb"))
        minChunkingSize = cmdOpts["minChunkingMb"].as<unsigned>() * 1024 * 1024ull;
    if (cmdOpts.count("minChunkingBytes"))
        minChunkingSize += cmdOpts["minChunkingBytes"].as<unsigned>();
    size_t chunkSize = 0;
    if (cmdOpts.count("chunkSizeMb"))
        chunkSize = cmdOpts["chunkSizeMb"].as<unsigned>() * 1024 * 1024ull;
    if (cmdOpts.count("chunkSizeBytes"))
        chunkSize += cmdOpts["chunkSizeBytes"].as<unsigned>();   
    sdk.enableMessageChunking(minChunkingSize,chunkSize);
}

int
main(int argc, char* argv[])
{

    bpo::options_description flags;
    bpo::variables_map cmdOpts;

    try {
        parseCmdLine(argc, argv, flags, cmdOpts);
    } catch(std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    } catch(...) {
        std::cerr << "Exception of unknown type!" << std::endl;;
        return 1;
    }

    if (cmdOpts.count("negotiate-error")) {
        int* errorCheckingConnections = (int*)dlsym(RTLD_DEFAULT, "errorCheckingConnections");
        if (errorCheckingConnections == nullptr) {
            fprintf(stderr,"Error getting symbol \"errorCheckingConnections\".\n");
            fprintf(stderr,"libarras4_client needs to be build with it enabled\n");
            exit(1);
        } else {
            *errorCheckingConnections = cmdOpts["negotiate-error"].as<unsigned>();
        }
    }

    if (cmdOpts.count("help")) {
        std::cout << flags << std::endl;
        return 0;
    }

    std::shared_ptr<arras4::sdk::SDK> pSdk = std::make_shared<arras4::sdk::SDK>();
    pSdk->setMessageHandler(&messageHandler);
    pSdk->setStatusHandler(&statusHandler);
    pSdk->setExceptionCallback(&exceptionCallback);
 
    initChunking(*pSdk, cmdOpts);

    std::string expectedStatusFilename = cmdOpts["expected-status"].as<std::string>();
    std::string expectedStatusString;
    if (!expectedStatusFilename.empty() && (expectedStatusFilename != "")) {
        Json::Value expectedStatusJson;
        std::ifstream json_file(expectedStatusFilename, std::ifstream::binary);
        std::ostringstream expectedStatusStream;
        expectedStatusStream << json_file.rdbuf();
        expectedStatusString = expectedStatusStream.str();
    }

    arras4::log::Logger::Level logLevel;
    auto ll = cmdOpts["log-level"].as<unsigned short>();
    if (ll <= arras4::log::Logger::LOG_TRACE) {
        logLevel = static_cast<arras4::log::Logger::Level>(ll);
    } else {
        std::cerr << "Supported log levels are 0-5" << std::endl;
        return 1;
    }
    int traceLevel = cmdOpts["trace-level"].as<int>();
    arras4::log::AthenaLogger& logger = static_cast<arras4::log::AthenaLogger&>(arras4::log::Logger::instance());
    logger.setThreshold(logLevel);
    logger.setTraceThreshold(traceLevel);
    logger.setProcessName("client");
    logger.setThreadName("main");

    unsigned int log_size = cmdOpts["large-log"].as<unsigned>();
    if (log_size > 0) {
        std::string logline;
        logline.resize(log_size,'*');
        try {
            ARRAS_LOG_INFO(logline);
        } catch (const std::exception& e) {
            ARRAS_LOG_ERROR("Got an exception logging: %s", e.what());
        }
    }

    bool expectConnectError= cmdOpts.count("expect-connect-error");
    bool ok = runSession(*pSdk, cmdOpts);

    int exitCode = 0;
    bool expectDisconnect = cmdOpts.count("expect-disconnect");
    if (pSdk->isConnected()) {
        ARRAS_LOG_INFO("Shutting down session"); 
        try {
            pSdk->shutdownSession();
        } catch (const arras4::client::ClientException& e) {
            if (expectDisconnect) {
                ARRAS_LOG_INFO("Got an exception shutting down as expected: %s", e.what());
            } else {
                ARRAS_LOG_ERROR("Got an unexpected exception shutting down: %s", e.what());
                exitCode = ERROR_EXIT_CODE;
            }
        }
    } else {
        ARRAS_LOG_WARN("NOTE: The session was disconnected by Arras during the run");
    }

    if (!pSdk->waitForDisconnect(MAX_WAIT_FOR_DISCONNECT_SECS)) {
        ARRAS_LOG_WARN("Arras failed to disconnect within %d seconds",MAX_WAIT_FOR_DISCONNECT_SECS);
           exitCode = ERROR_EXIT_CODE;
    }


    if (!shutdownStatus.empty()) {
        ARRAS_LOG_INFO("Shutdown status = %s", shutdownStatus.c_str());
    }

    if (!expectedStatusString.empty() && (expectedStatusString != "")) {
        ARRAS_LOG_INFO("Expected shutdown status = %s", expectedStatusString.c_str());
        if (shutdownStatus.empty()) {
            ARRAS_LOG_ERROR("Didn't get a shutdown status for session when one was expected");
            exitCode = ERROR_EXIT_CODE;
        } else if (!subset(expectedStatusString, shutdownStatus)) {

            ARRAS_LOG_ERROR("Unexpected shutdown status");
            exitCode = ERROR_EXIT_CODE;
        }
    }

    if (!ok && !expectConnectError) {
        ARRAS_LOG_ERROR("Got an unexpected connect error");
        exitCode = ERROR_EXIT_CODE;
    } else if (ok && expectConnectError) {
        ARRAS_LOG_ERROR("Didn't get an expected onnect error");
        exitCode = ERROR_EXIT_CODE;
    }

    if (arrasExceptionThrown && !expectDisconnect) {
        ARRAS_LOG_WARN("NOTE: an Arras exception was thrown during the run");
        exitCode = ERROR_EXIT_CODE;
    }

    if (arrasUnexpectedStop && !expectDisconnect) {
        ARRAS_LOG_WARN("NOTE: the session stopped unexpectedly during the run");
        exitCode = ERROR_EXIT_CODE;
    }

    if (cmdOpts.count("get-logs")) {
        // allow some time for the logging system to get the results
        if (sessionIdStr.empty()) {
            printf("There was no session id\n");
        } else {
            // allow some time for the logs to make it to the server
            sleep(10);
            std::string cmd_dc = cmdOpts["dc"].as<std::string>();
            std::string cmd_env = cmdOpts["env"].as<std::string>();
            if (cmd_env == "local") {
                cmd_env = "prod";
            }
            std::string coordinator;
            std::string logs;
            std::string consul;
            initServiceUrls(cmd_dc, cmd_env, coordinator, logs, consul);

            std::vector<Session> sessionVector;
            Session session;
            session.mId = sessionIdStr;
            sessionVector.push_back(session);;
             getLogs( logs, sessionVector, 2000);
             int size = sessionVector[0].mLogLines.size();
             if (size > 0) {
                printf("******************************************************************************\n");
                printf("******* Start of logs for session %s *******\n", sessionIdStr.c_str());
                printf("******************************************************************************\n");
                for (auto i=0; i < size; i++) {
                    printf("%s\n", sessionVector[0].mLogLines[i].c_str());
                }
                printf("*****************************\n");
                printf("******** End of logs ********\n");
                printf("*****************************\n");
            }
        }
    }
    return exitCode;
}

