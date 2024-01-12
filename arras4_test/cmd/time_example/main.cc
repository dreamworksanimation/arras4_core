// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0


// need for ARRAS logging macros (ARRAS_DEBUG, ARRAS_INFO, etc)
#include <arras4_log/Logger.h>

// need for boost::to_lower
#include <boost/algorithm/string.hpp>

// needed for parsing command line options
#include <boost/program_options.hpp>
namespace bpo = boost::program_options;

#include <cstdlib> // std::getenv

// needed for parse and printing the json session status
#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>

// needed for arras4::api::Message
#include <message_api/Message.h>

// needed for arras4::sdk::SDK, arras4::client::SessionDefinition,
// arras4::client::DefinitionLoadError, and arras4::client::SessionOptions
#include <sdk/sdk.h>

#include <string>

// needed for TimeExampleMessage
#include <time_example_message/TimeExampleMessage.h>

#include <unistd.h> // sleep


unsigned short constexpr DEFAULT_RUNTIME_SECS = 15;
unsigned short constexpr DEFAULT_LOG_LEVEL = 2;
unsigned short constexpr DEFAULT_ACAP_PORT = 8080;

const std::string DEFAULT_SESSION_NAME = "time_example";
const std::string DEFAULT_ENV_NAME = "prod";


const std::string
getStudioName()
{
    std::string studio = std::getenv("STUDIO");
    boost::to_lower(studio);
    return studio;
}

void
parseCmdLine(int argc, char* argv[],
             bpo::options_description& flags, bpo::variables_map& cmdOpts) {
    flags.add_options()
        ("help", "produce help message")
        ("env", bpo::value<std::string>()->default_value("prod"))
        ("dc", bpo::value<std::string>()->default_value(getStudioName()))
        ("host", bpo::value<std::string>(), "ACAP host name, if unspecified ACAP will be located using the studio's config service")
        ("port", bpo::value<unsigned short>()->default_value(DEFAULT_ACAP_PORT), "ACAP port number, ignored unless --host is specified")
        ("runtime", bpo::value<unsigned short>()->default_value(DEFAULT_RUNTIME_SECS), "Runtime")
        ("session,s", bpo::value<std::string>()->default_value(DEFAULT_SESSION_NAME), "Name of Arras session to use")
        ("production,p", bpo::value<std::string>()->default_value(std::string()), "Production")
        ("sequence", bpo::value<std::string>()->default_value(std::string()), "Sequence")
        ("shot", bpo::value<std::string>()->default_value(std::string()), "Shot")
        ("assetGroup", bpo::value<std::string>()->default_value(std::string()), "Asset Group")
        ("asset", bpo::value<std::string>()->default_value(std::string()), "Asset")
        ("department", bpo::value<std::string>()->default_value(std::string()), "Department")
        ("team", bpo::value<std::string>()->default_value(std::string()), "Team")
        ("log-level,l", bpo::value<unsigned short>()->default_value(DEFAULT_LOG_LEVEL), "Log level [0-5] with 5 being the highest")
        ("prepend", bpo::value<std::string>()->default_value(std::string()), "")
        ("local-only", "Require running on a local node")
    ;

    bpo::store(bpo::command_line_parser(argc, argv).
               options(flags).run(), cmdOpts);
    bpo::notify(cmdOpts);
}

//
// Status messages occur during an orderly shutdown providing final
// information about the session. In the future status messages might be sent
// at other times, like when requested. The statusHandler is registered during
// initialization of the SDK
//
void statusHandler(const std::string& status)
{
    Json::Reader reader;
    Json::Value jsonStatus;
    Json::StyledWriter writer;

    reader.parse(status, jsonStatus);

    std::string styledStatus = writer.write(jsonStatus);

    std::cout << "Final session status: " << styledStatus;
}

//
// Incoming messages are delivered to the messageHander which was registered
// during initialiation of the SDK
//
void messageHandler(const arras4::api::Message& message)
{
    if (message.classId() == TimeExampleMessage::ID) {
        TimeExampleMessage::ConstPtr timeMessage = message.contentAs<TimeExampleMessage>();
        std::cout << timeMessage->getValue() << std::endl;
    } else {
        std::cerr << "Received an unexpected message type " << message.classId() << std::endl;
    }
}

//
// Since the message receiver runs in a separate thread exceptions need to be
// delivered through a callback which is registered during intialization of
// the SDK.
//
void exceptionCallback(const std::exception& e)
{
    ARRAS_LOG_ERROR("Thrown exception in message reciever thread: %s", e.what());
}

int
main(int argc, char* argv[]) {

    bpo::options_description flags;
    bpo::variables_map cmdOpts;

    //
    // command line parsing
    //
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
    // set up logger
    //
    // If a logger isn't explicitly set it will be initalized to
    // SDK::configAthenaLogger("prod", true /* use color */, "localhost", 514);
    // the first time an SDK is constructed
    //
    arras4::log::Logger::Level logLevel;
    auto ll = cmdOpts["log-level"].as<unsigned short>();
    if (ll <= arras4::log::Logger::LOG_TRACE) {
        logLevel = static_cast<arras4::log::Logger::Level>(ll);
    } else {
        std::cerr << "Supported log levels are 0-5" << std::endl;
        return 1;
    }
    arras4::sdk::SDK::configAthenaLogger("prod"); 
    arras4::log::Logger& logger = arras4::log::Logger::instance();
    logger.setThreshold(logLevel);    // defaults to LOG_WARN
    logger.setProcessName("example client"); // defaults to "client"
    logger.setThreadName("main");     // defaults to thread id number

    
    //
    // Initialize an arras SDK
    //
    arras4::sdk::SDK* pSdk = new arras4::sdk::SDK();
    pSdk->setStatusHandler(&statusHandler);
    pSdk->setMessageHandler(&messageHandler);
    pSdk->setExceptionCallback(&exceptionCallback);

    //
    // Construct a url for talking to the Arras coordinator.
    // SDK::requestArrasUrl will throw a ClientException
    // (derived from std::exception) if the configuration server can't be
    // contacted or if the datacenter or enviroment are invalid.
    //
    std::string arrasUrl;
    if (cmdOpts.count("host")) {
        const std::string cmd_host = cmdOpts["host"].as<std::string>();
        const unsigned short cmd_port = cmdOpts["port"].as<unsigned short>();
        arrasUrl = std::string("http://") + cmd_host + ":" + std::to_string(cmd_port) + "/coordinator/1/sessions";
    } else {
        std::string cmd_dc = cmdOpts["dc"].as<std::string>();
        std::string cmd_env = cmdOpts["env"].as<std::string>();
        arrasUrl = pSdk->requestArrasUrl(cmd_dc, cmd_env);
    }
    std::cout << "Using ARRAS URL " << arrasUrl << " to create session." << std::endl;

    //
    // Set up SessionOptions with information about what the
    // user is working on. These can influence which resources are available.
    // Each of the set functions returns a SessionOptions object
    // so set calls can be chained.
    //
    auto sessionOptions = arras4::client::SessionOptions().\
        setProduction(cmdOpts["production"].as<std::string>()).\
        setSequence(cmdOpts["sequence"].as<std::string>()).\
        setShot(cmdOpts["shot"].as<std::string>()).\
        setAssetGroup(cmdOpts["assetGroup"].as<std::string>()).\
        setAsset(cmdOpts["asset"].as<std::string>()).\
        setDepartment(cmdOpts["department"].as<std::string>()).\
        setTeam(cmdOpts["team"].as<std::string>());

    //
    // Read a JSON session definition from a file. It will search the directories
    // listed in the ARRAS_SESSION_PATH environment variable for
    // {sessionName}.sessionDef. the package.yaml for arras4_test adds
    // arras4_test/{version}/sessions to the ARRAS_SESSION_PATH
    //
    // It will throw a arras4::client::DefinitionLoadError
    // (derived from std::exception) if it can't load the file or if there is
    // a JSON parsing error
    //
    std::string sessionName = cmdOpts["session"].as<std::string>();
    arras4::client::SessionDefinition sessionDef = arras4::client::SessionDefinition::load(sessionName);

    //
    // prepend an additional rez directory in the session definition to allow a
    // test map or development rez directory to be used for the computation and
    // it's environment
    //
    sessionDef["time_example_comp"]["requirements"]["rez_packages_prepend"] = cmdOpts["prepend"].as<std::string>();

    // optionally indicate that the computation must be on the node local to the client
    if (cmdOpts.count("local-only")) {
        sessionDef["time_example_comp"]["requirements"]["local_only"] = std::string("yes");
    }

    //
    // Create the session. This will throw an exception if it can't contact the
    // coordination for the request, if there aren't enough resources, or if
    // these is a problem contacting the "entry node".
    //
    const std::string sessionId = pSdk->createSession(sessionDef, arrasUrl, sessionOptions);
    std::cout << "The session id is " << sessionId << std::endl;

    //
    // Wait for the session to be ready
    //
    const int timeoutInSeconds = 30;
    bool engineReadyStatus = pSdk->waitForEngineReady(timeoutInSeconds);
    if (!engineReadyStatus) {
        std::cerr << "The session isn't ready after " << timeoutInSeconds << "second. Giving up." << std::endl;
    }

    //
    // Do the work of the application, in this case a trivial loop over
    // sending requests for the time. The responses are printed in the message
    // handler.
    //
    auto runtime = cmdOpts["runtime"].as<unsigned short>();
    bool done = false;
    auto startTime = std::chrono::steady_clock::now();
    double elapsedSeconds;
    do {
        sleep(1);
        TimeExampleMessage::Ptr tm = std::make_shared<TimeExampleMessage>();
        tm->setValue("The time on the arras server is");
        pSdk->sendMessage(tm);

        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double,std::micro> elapsedDuration = (currentTime - startTime);
        elapsedSeconds = elapsedDuration.count() / 1000000.0;
    } while (elapsedSeconds < runtime);
    

    //
    // Cleanly shut down the session.
    // By shutting down cleanly it is clearer that the session was ended
    // intentially rather than the client crashing. It also provides an
    // opportunity to get final status information on the session which
    // is sent to the status handler function
    //
    if (pSdk->isConnected()) {
        pSdk->shutdownSession();

        // wait for ARRAS to shut down the connection
        bool disconnectStatus = pSdk->waitForDisconnect(timeoutInSeconds);
        if (!disconnectStatus) {
            // ARRAS didn't disconnect so disconnect from our end
            std::cerr << "Unable to shutdown ARRAS session cleanly. Force a disconnect." << std::endl;
            pSdk->disconnect();
            return 1;
        }
    } else {
        std::cerr << "ARRAS session disconnected unexpectedly." << std::endl;
        return 1;
    }

    return 0;
}

