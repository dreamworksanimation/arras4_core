// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0



#include <arras4_log/Logger.h>
#include <atomic>
#include <benchmark_computation/Credits.h>
#include <benchmark_message/BenchmarkMessage.h>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <client/api/SessionDefinition.h>
#include <iomanip>
#include <json/json.h>
#include <message_api/messageapi_names.h>
#include <message_api/Message.h>
#include <message_api/Object.h>
#include <mutex>
#include <sdk/sdk.h>
#include <sstream>

#if defined(JSONCPP_VERSION_MAJOR) // Early version of jsoncpp don't define this.
// Newer versions of jsoncpp renamed the memberName method to name.
#define memberName name
#endif

// options for describing that the message should be sent
// as "For_benchcomp0" to allow it to go to only the one computation
arras4::api::Object computationoptions[16];

namespace bpo = boost::program_options;
using namespace arras4::benchmark;

unsigned short constexpr DEFAULT_COORDINATOR_PORT = 8087;
const std::string DEFAULT_COORDINATOR_PATH = "/coordinator/1/sessions";
const std::string DEFAULT_ENV_NAME = "stb";
const std::string DEFAULT_SESSION_NAME = "benchmark_test";
unsigned short constexpr MAX_WAIT_FOR_READY_SECS = 120;
unsigned constexpr DEFAULT_STAY_CONNECTED_SECS = 10;
unsigned short constexpr DEFAULT_LOG_LEVEL = 1;
int constexpr ERROR_EXIT_CODE = 1;

// The following variables are global because they
// are access both in the main thread (runsession) and in the message
// handler.

unsigned cmd_delayStart;
std::string cmd_prepend;
std::string cmd_packaging_system;
unsigned cmd_cores;
unsigned cmd_requestMb;
unsigned cmd_threads;
unsigned cmd_allocateMb;
unsigned cmd_touchMb;
unsigned cmd_touchOnce;
unsigned cmd_logThreads;
unsigned cmd_logCount;
unsigned int cmd_messageSleep;
std::string cmd_host;
unsigned short cmd_port;
std::string cmd_dc;
std::string cmd_env;
bool cmd_disconnectImmediately;
bool cmd_noTimeout;
bool cmd_allowDisconnect;
bool cmd_localOnly;
unsigned cmd_phasedStart;

class SessionInstance {
  public:
    SessionInstance() :
        credit(5),
        arrasStopped(false),
        arrasExceptionThrown(false),
        receivedSessionStatus(false),
        exitCode(0),
        mGotException(false),
        mInstanceIndex(0) {}
    std::shared_ptr<arras4::sdk::SDK> pSdk;
    std::mutex sdkMutex;

    // the number of messages which have been acknowledged. 
    // This will be reset at the beginning of the benchmark
    std::atomic<unsigned long> acksSent;
    std::atomic<unsigned long> acksReceived;

    // This keeps track of the number of messages which can be
    // sent without acknowledgment.
    Credits credit;

    std::atomic<bool> arrasStopped;
    std::atomic<bool> arrasExceptionThrown;

    bool receivedSessionStatus;
    std::string sessionStatus;

    int exitCode;

    std::thread mThread;
    std::string mSessionId;

    bool mGotException;
    
    int mInstanceIndex;

    std::chrono::time_point<std::chrono::steady_clock> mStartTime;
    std::chrono::time_point<std::chrono::steady_clock> mCreateTime;
    std::chrono::time_point<std::chrono::steady_clock> mReadyTime;

    void messageHandler(const arras4::api::Message& message);
    void statusHandler(const std::string& status);
    void exceptionCallback(const std::exception& e);
};

SessionInstance* sessions;

const std::string getStudioName()
{
    std::string studio = std::getenv("STUDIO");
    boost::to_lower(studio);
    return studio;
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
        ("repeat,r",bpo::value<unsigned>()->default_value(1), "Number of times to do the test")
        ("sessions",bpo::value<unsigned>()->default_value(1), "Number of parallel sessions to run the operation with")
        ("interval,i",bpo::value<unsigned>()->default_value(1), "Interval between message sends (seconds)")
        ("minChunkingMb",bpo::value<unsigned>(),"Minimum message size for chunking (megabytes)")
        ("minChunkingBytes",bpo::value<unsigned>(),"Minimum message size for chunking (bytes)")
        ("chunkSizeMb",bpo::value<unsigned>(),"Chunk size (megabytes)")
        ("chunkSizeBytes",bpo::value<unsigned>(),"Chunk size (bytes)")
        ("bandwidthPath",bpo::value<std::string>()->default_value(""),"Path to test bandwidth on")
        ("credits",bpo::value<unsigned>()->default_value(1),"Number of unacknowledged messages which are allowed")
        ("duration",bpo::value<unsigned>()->default_value(30),"duration of test in seconds")
        ("report-frequency",bpo::value<unsigned>()->default_value(5),"report frequency in seconds")
        ("cores",bpo::value<unsigned>(),"Number of cores to ask for for the computation")
        ("threads",bpo::value<unsigned>()->default_value(0),"Number of threads which should be actively computing")
        ("requestMb",bpo::value<unsigned>()->default_value(2048),"Amount of memory to request for the computation")
        ("allocateMb",bpo::value<unsigned>()->default_value(0),"Amount of memory that the computation should allocate")
        ("touchMb",bpo::value<unsigned>()->default_value(0),"Amount of memory that the computation should actually touch")
        ("touchOnce",bpo::value<unsigned>()->default_value(0),"Whether to only write the memory once rather than continuously")
        ("logThreads",bpo::value<unsigned>()->default_value(12),"The number of threads to do logging from")
        ("logCount",bpo::value<unsigned>()->default_value(0),"The number of log messages to generate")
        ("messageSleep",bpo::value<unsigned>()->default_value(0),"The number microseconds to sleep in messageHandler")
        ("prepend", bpo::value<std::string>(), "rez directory to prepend to computation environment")
        ("printEnv", "Print all of the environment variables set in the computation")
        ("noTimeout", "Print all of the environment variables set in the computation")
        ("allowDisconnect", "Don't consider a disconnect a test failure")
        ("local-only", "Only allow computations to run on the local node")
        ("delay-start", bpo::value<unsigned>()->default_value(0), "The number seconds to wait before starting the test")
        ("ignore-errors", "Ignore errors and keep retrying")
        ("phased-start", bpo::value<unsigned>()->default_value(0), "Seconds between start times of test threads (default 0)")
        ("packaging-system", bpo::value<std::string>(), "Packaging system to specify in the session configuration")
        ;

    bpo::store(bpo::command_line_parser(argc, argv).
               options(flags).run(), cmdOpts);
    bpo::notify(cmdOpts);
}

//
// Get the coordinator url fro the config service or contruct it from the provided
// command line options
//
std::string getCoordinatorUrl(arras4::sdk::SDK& sdk)
{


    std::string url;
    if (!cmd_host.empty()) {
        std::ostringstream ss;
        ss << "http://" << cmd_host
           << ":" << cmd_port
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
             const std::string& sessionName, SessionInstance& session)
{
    int index = session.mInstanceIndex;

    try {

        // sessionName is has ".sessiondef" and the directories in the environment variable ARRAS_SESSION_PATH
        // are searched for a session definition
        arras4::client::SessionDefinition def = arras4::client::SessionDefinition::load(sessionName);



        arras4::api::ObjectConstRef computations = def.getObject()["computations"];
        if (cmd_cores > 0.0) {
            def["benchcomp0"]["requirements"]["resources"]["cores"] = float(cmd_cores);
        }
        def["benchcomp0"]["requirements"]["resources"]["memoryMB"] = cmd_requestMb;
        if (cmd_localOnly) {
            if (computations.isMember("benchcomp0")) def["benchcomp0"]["requirements"]["local_only"] = std::string("yes");
            if (computations.isMember("benchcomp1")) def["benchcomp1"]["requirements"]["local_only"] = std::string("yes");
        }
        def["benchcomp0"]["threads"] = cmd_threads;
        def["benchcomp0"]["allocateMb"] = cmd_allocateMb;
        def["benchcomp0"]["touchMb"] = cmd_touchMb;
        def["benchcomp0"]["touchOnce"] = cmd_touchOnce;
        def["benchcomp0"]["logThreads"] = cmd_logThreads;
        def["benchcomp0"]["logCount"] = cmd_logCount;
        if (!cmd_prepend.empty()) {
            if (computations.isMember("benchcomp0")) def["benchcomp0"]["requirements"]["rez_packages_prepend"] = cmd_prepend;
            if (computations.isMember("benchcomp1")) def["benchcomp1"]["requirements"]["rez_packages_prepend"] = cmd_prepend;
        }
        if (!cmd_packaging_system.empty()) {
            if (computations.isMember("benchcomp0")) def["benchcomp0"]["requirements"]["packaging_system"] = cmd_packaging_system;
            if (computations.isMember("benchcomp1")) def["benchcomp1"]["requirements"]["packaging_system"] = cmd_packaging_system;
        }

        std::string coordinatorUrl = getCoordinatorUrl(sdk);

        arras4::client::SessionOptions so;

        fprintf(stderr,"coordinator url = %s\n", coordinatorUrl.c_str());

        Json::StyledWriter writer;

        std::cout << writer.write(def.getObject());
        sdk.createSession(def, coordinatorUrl, so);

        ARRAS_LOG_INFO("Created session with ID %s",sdk.sessionId().c_str());

        if (cmd_disconnectImmediately) {
            ARRAS_LOG_WARN("--disconnectImmediately specified : disconnecting now for testing");
            sdk.disconnect();
        }

    } catch (arras4::sdk::SDKException& e) {
        ARRAS_LOG_ERROR("Unable to SDK::createSession: index %d %s", index, e.what());
        return false;
    } catch (arras4::client::DefinitionLoadError& e) {
        ARRAS_LOG_ERROR("Failed to load session: index %d %s", index, e.what());
        return false;
    } catch (...) {
        ARRAS_LOG_ERROR("Unknown exception thrown from SDK::createSession: index %d", index);
    }
    return true;
}

//
// Construct a message from the string and send the message
//
// Separate version which grows the string to a given size to create a payload
//
void sendBenchmarkMessage(SessionInstance& session, BenchmarkMessage::MessageType type, int index, const std::string& value=std::string()) 
{
    std::unique_lock<std::mutex> lock(session.sdkMutex);

    BenchmarkMessage::Ptr tm = std::make_shared<BenchmarkMessage>(type, value, "client");
    try {
        session.pSdk->sendMessage(tm, computationoptions[index]);
    } catch (const arras4::client::ClientException& e) {
        fprintf(stderr,"sendBenchmarkMessage(0): Thread %d : Got a ClientException\n", session.mInstanceIndex);
        throw e;
    }
}

void sendBenchmarkMessage(SessionInstance& session, BenchmarkMessage::MessageType type, int index, const std::string& value, unsigned int size) 
{
    std::unique_lock<std::mutex> lock(session.sdkMutex);

    BenchmarkMessage::Ptr tm = std::make_shared<BenchmarkMessage>(type, value, "client");
    tm->mValue.resize(size, '*');
    try {
        session.pSdk->sendMessage(tm, computationoptions[index]);
    } catch (const arras4::client::ClientException& e) {
        fprintf(stderr,"sendBenchmarkMessage(1): Thread %d : Got a ClientException\n", session.mInstanceIndex);
        throw e;
    }
}

//
// Asynchronously handle an incoming message
//
void SessionInstance::messageHandler(const arras4::api::Message& message)
{
    if (cmd_messageSleep) {
        usleep(cmd_messageSleep);
    }
    SessionInstance& session = sessions[mInstanceIndex];
    if (message.classId() == BenchmarkMessage::ID) {
        BenchmarkMessage::ConstPtr bm = message.contentAs<BenchmarkMessage>();
        if (bm->mType == BenchmarkMessage::MessageType::ACK) {
            session.credit.increment();
            session.acksReceived++;
        } else if (bm->mType == BenchmarkMessage::MessageType::SEND_ACK) {
            try {
                sendBenchmarkMessage(session, BenchmarkMessage::MessageType::ACK, 0, "");
            } catch (const arras4::client::ClientException& e) {
                fprintf(stderr,"messageHandler: Thread %d: Got ClientException sending ACK: %s\n", session.mInstanceIndex, e.what());
                session.mGotException = true;
            }
            session.acksSent++;
        } else if (bm->mType == BenchmarkMessage::MessageType::REPORT) {
            std::cout << bm->mValue << "\n";
        }
    }
}

const char* normalSessionStatus=
"{\n"
"  \"clientDisconnectReason\": \"shutdown\",\n"
"  \"clientStatus\": \"connected\",\n"
"  \"computations\": [\n"
"    {\n"
"      \"compStatus\": \"\",\n"
"      \"execStatus\": \"stopped\",\n"
"      \"hyperthreaded\": false,\n"
"      \"name\": \"benchcomp1\",\n"
"      \"signal\": \"not set\",\n"
"      \"stoppedReason\": \"terminated as requested\"\n"
"    },\n"
"    {\n"
"      \"compStatus\": \"\",\n"
"      \"execStatus\": \"stopped\",\n"
"      \"hyperthreaded\": false,\n"
"      \"name\": \"benchcomp0\",\n"
"      \"signal\": \"not set\",\n"
"      \"stoppedReason\": \"terminated as requested\"\n"
"    }\n"
"  ],\n"
"  \"execStatus\": \"stopped\",\n"
"  \"execStoppedReason\": \"clientShutdown\",\n"
"}\n";

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
                    ARRAS_LOG_ERROR("%s doesn't match (%s != %s)\n",
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
                    ARRAS_LOG_ERROR("The array sizes of %s don't match (%s != %s)",
                                     aVarName.c_str(), size, aSuperset.size());
                    return false;
                }
            }
            for (auto i = 0u; i < size; i++) {

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
            auto iter = aSubset.begin();
            while (iter != aSubset.end()) {
                std::string name = aVarName + "." + iter.memberName();
                Json::Value value = aSuperset[iter.memberName()];
                if (value.isNull()) {
                    if (aPrintError) {
                        ARRAS_LOG_ERROR("Superset is missing %s", name.c_str());
                        return false;
                    }
                }
                if (!subset(*iter, value, name, aPrintError)) return false;
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

    return subset(subsetJson, supersetJson, "", true);
}

//
// special function for handling a status message. The status
// is already pulled out of the message as a json string
//
void
SessionInstance::statusHandler(const std::string& status)
{
    SessionInstance& session = sessions[mInstanceIndex];
    session.sessionStatus = status;
    session.receivedSessionStatus = true;
}

//
// The exception callback is called when there is an exception thrown inside of the
// message handling thread
//
void
SessionInstance::exceptionCallback(const std::exception& e)
{
    SessionInstance& session = sessions[mInstanceIndex];
    fprintf(stderr,"exceptionCallback: Thread %d: Thrown exception: %s", mInstanceIndex, e.what());
    session.arrasExceptionThrown = true;
}

namespace {

std::string bandwidth_report(
    float seconds,
    unsigned long messages,
    unsigned long data)
{
    std::ostringstream reportOut;
    const float fmessages = static_cast<float>(messages);
    const float fdata = static_cast<float>(data);

    reportOut << "Time: ";
    reportOut << std::fixed << std::setprecision(2) << seconds;
    reportOut << "s Msgs: ";
    reportOut << messages;
    reportOut << " Rate: ";
    reportOut << std::fixed << std::setprecision(2) << (fmessages / seconds);
    reportOut << "msg/s (";
    reportOut << std::fixed << std::setprecision(2) << 1.0f/(fmessages / seconds)*1000000.0f;
    reportOut << "µs) ";
    reportOut << std::fixed << std::setprecision(2) << (fdata / 1048576.0f / seconds);
    reportOut << "MB/s";

    return reportOut.str();
}

}

std::string sessionName;
unsigned stayConnectedSecs;
size_t dataSize;
unsigned interval;
unsigned credits;
std::string bandwidthPath;
unsigned duration;
unsigned reportFrequency;
unsigned logCount;
bool printEnv;


bool runSession(SessionInstance& session)
{
    session.mGotException = false;
    session.arrasExceptionThrown = false;

    fprintf(stderr, "sessionName = %s\n", sessionName.c_str());

    session.mStartTime = std::chrono::steady_clock::now();
    if (!connect(*session.pSdk, sessionName, session)) {
        ARRAS_LOG_ERROR("Exiting due to failed connection, index %d", session.mInstanceIndex);
        return false;
    }
    session.mCreateTime = std::chrono::steady_clock::now();
    auto elapsedTime = session.mCreateTime- session.mStartTime;
    std::chrono::duration<double, std::micro> elapsedDuration = elapsedTime;
    double elapsedSeconds = elapsedDuration.count() / 1000000.0;

    session.mSessionId = session.pSdk->sessionId();

    fprintf(stderr,"index %d sessionId = %s (took %f seconds to create)\n", session.mInstanceIndex, session.pSdk->sessionId().c_str(), elapsedSeconds);

    if (cmd_noTimeout) {
        int waitTime = 0;
        while (!session.pSdk->waitForEngineReady(30)) {
            waitTime += 30;
            fprintf(stderr,"Thread %d: session %s hasn't send engine ready after %d seconds", session.mInstanceIndex, session.mSessionId.c_str(), waitTime);
        }
    } else {
        bool ready = session.pSdk->waitForEngineReady(MAX_WAIT_FOR_READY_SECS);
        if (!ready) {
            ARRAS_LOG_ERROR("Thread %d: session %s Exiting : timed out waiting for engine to be ready", session.mInstanceIndex, session.mSessionId.c_str());
            return false;
        }
    }
    session.mReadyTime = std::chrono::steady_clock::now();
    elapsedTime = session.mReadyTime - session.mCreateTime;
    elapsedDuration = elapsedTime;
    elapsedSeconds = elapsedDuration.count() / 1000000.0;


    fprintf(stderr,"index %d took %f seconds from started to ready\n", session.mInstanceIndex, elapsedSeconds);
    ARRAS_LOG_INFO("Client connected OK");
    if (cmd_delayStart) {
        sleep(cmd_delayStart);
    }

    if (printEnv) {
        session.acksSent = 0;
        sendBenchmarkMessage(session, BenchmarkMessage::MessageType::PRINTENV, 0, "");
        while (session.acksReceived == 0) {
            usleep(10000);
        }
    }
    if (bandwidthPath == "client_to_computation") {
        unsigned long lastMessages=0;
        auto startTime = std::chrono::steady_clock::now();
        auto lastReport = startTime;
        unsigned long sentMessages(0);
        session.credit.set(credits);
        while (1) {
            sendBenchmarkMessage(session, BenchmarkMessage::MessageType::SEND_ACK, 0,
                                 "", static_cast<unsigned int>(dataSize));
            session.credit.waitAndDecrement();

            sentMessages++;

            auto currentTime = std::chrono::steady_clock::now();
            unsigned long currentMessages = sentMessages;
            auto deltaTime = currentTime - lastReport;
            if ((currentTime - lastReport) > std::chrono::seconds(reportFrequency)) {
                unsigned long deltaMessages = currentMessages - lastMessages;
                lastMessages = currentMessages;
                lastReport = currentTime;

                std::chrono::duration<double, std::micro> elapsedDuration = deltaTime;
                float elapsedSeconds = elapsedDuration.count() / 1000000.0f;
                std::string report = bandwidth_report(elapsedSeconds, deltaMessages, deltaMessages * dataSize);
                report += " TOTALS: ";

                auto elapsedTime = currentTime - startTime;
                elapsedDuration = elapsedTime;
                elapsedSeconds = elapsedDuration.count() / 1000000.0f;
                report += bandwidth_report(elapsedSeconds, currentMessages, currentMessages * dataSize);
                std::cout << report << "\n";

                if (elapsedTime > std::chrono::seconds(duration)) break;
            }
        }
        sendBenchmarkMessage(session, BenchmarkMessage::MessageType::STOP, 0);
    } else if (bandwidthPath == "computation_to_client") {
        unsigned long lastMessages=0;
        session.acksSent = 0;

        std::string parameters= std::to_string(credits) + " " + std::to_string(dataSize) + " For_client";
        sendBenchmarkMessage(session, BenchmarkMessage::MessageType::START_STREAM_OUT, 0, parameters);

        auto startTime = std::chrono::steady_clock::now();
        auto lastReport = startTime;
        
        while (1) {
            // there is no real work to do other than let messages be streamed from the computation
            // and be acknowledged in the message handler so just sleep
            sleep(1);

            if (session.mGotException) return cmd_allowDisconnect;

            auto currentTime = std::chrono::steady_clock::now();
            unsigned long currentMessages = session.acksSent;
            auto deltaTime = currentTime - lastReport;
            if (deltaTime > std::chrono::seconds(reportFrequency)) {
                unsigned long deltaMessages = currentMessages - lastMessages;
                lastMessages = currentMessages;
                lastReport = currentTime;

                std::chrono::duration<double, std::micro> elapsedDuration = deltaTime;
                float elapsedSeconds = elapsedDuration.count() / 1000000.0f;
                std::string report = bandwidth_report(elapsedSeconds, deltaMessages, deltaMessages * dataSize);
                report += " TOTALS: ";

                auto elapsedTime = currentTime - startTime;
                elapsedDuration = elapsedTime;
                elapsedSeconds = elapsedDuration.count() / 1000000.0f;
                report += bandwidth_report(elapsedSeconds, currentMessages, currentMessages * dataSize);
                std::cout << report << "\n";

                if (elapsedTime > std::chrono::seconds(duration)) break;
            }
        }
        try {
            sendBenchmarkMessage(session, BenchmarkMessage::MessageType::STOP, 0);
        } catch (const arras4::client::ClientException& e) {
            fprintf(stderr,"Got an exception trying to end benchmark");
            session.mGotException = true;
            return false;
        }
    } else if (bandwidthPath == "computation_to_computation") {
        std::string parameters= std::to_string(credits) + " " + std::to_string(dataSize) + " For_benchcomp1";
        session.acksSent = 0;
        sendBenchmarkMessage(session, BenchmarkMessage::MessageType::START_STREAM_OUT, 0, parameters);
        int remaining = duration;
        while (remaining > 0) {
            unsigned int sleepTime = remaining;
            if (sleepTime > reportFrequency) sleepTime = reportFrequency;
            sleep(sleepTime);
            sendBenchmarkMessage(session, BenchmarkMessage::MessageType::SEND_REPORT, 0);
            remaining -= sleepTime;
        }
        sendBenchmarkMessage(session, BenchmarkMessage::MessageType::STOP, 0);
    } else if (bandwidthPath == "computations_to_computations") { 
        const int computations = 16;
        // for (auto i = 0; i < computations; i++) {
        for (auto i = 0; i < computations; i++) {
            std::string destinations;
            for (auto j = 0; j < computations; j++) {
                if (i!=j) {
                    destinations = destinations + ' ' + "For_benchcomp" + std::to_string(j);
                }
            }
            std::string parameters= std::to_string(credits) + " " + std::to_string(dataSize) + destinations;
            fprintf(stderr,"%s\n", parameters.c_str());

            session.acksSent = 0;
            sendBenchmarkMessage(session, BenchmarkMessage::MessageType::START_STREAM_OUT, i, parameters);
        }

        int remaining = duration;
        while (remaining > 0) {
            unsigned int sleepTime = remaining;
            if (sleepTime > reportFrequency) sleepTime = reportFrequency;
            sleep(sleepTime);
            for (auto i = 0; i < computations; i++) {
                sendBenchmarkMessage(session, BenchmarkMessage::MessageType::SEND_REPORT, i);
            }
            remaining -= sleepTime;
        }
        for (auto i = 0; i < computations; i++) {
            sendBenchmarkMessage(session, BenchmarkMessage::MessageType::STOP, i);
        }
    } else if (logCount > 0) {
        session.acksSent = 0;
        sendBenchmarkMessage(session, BenchmarkMessage::MessageType::LOGSPEED, 0, "");
        while (session.acksReceived == 0) {
            usleep(10000);
        }
    } else if (bandwidthPath == "") {
        sleep(duration);
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

void sessionThread(int index, int repeat, bool ignoreErrors)
{
    fprintf(stderr,"sessionThread %d\n",index);
    SessionInstance& session = sessions[index];

    for (int i=0; i < repeat; i++) {
        session.exitCode = 0;

        if (((i+1) % 10) == 0) fprintf(stderr,"Thread %d: Iteration %d\n", index, i+1);
        bool ok = runSession(session);

        if (!ok) {
            session.exitCode = ERROR_EXIT_CODE;
            if (ignoreErrors) continue;
            return;
        }

        if (session.pSdk->isConnected()) {
            try {
                session.pSdk->shutdownSession();
            } catch (const arras4::client::ClientException& e) {
                session.mGotException = true;
                if (!cmd_allowDisconnect) {
                    fprintf(stderr,"Got an exception attempting to shutdown the session");
                }
            }
            if (cmd_noTimeout) {
                int waitTime = 0;
                while (!session.pSdk->waitForDisconnect(30)) {
                    waitTime += 30;
                    fprintf(stderr,"Thread %d: session %s hasn't shut down after %d seconds", index, session.mSessionId.c_str(), waitTime);
                }
            } else {
                if (!session.pSdk->waitForDisconnect(30)) {
                    session.pSdk->disconnect();
                    fprintf(stderr,"Thread %d: session %s would not shut down cleanly", index, session.mSessionId.c_str());
                }
            }
        } else {
            ARRAS_LOG_ERROR("NOTE: The session was disconnected by Arras during the run");
        }
        if (session.arrasExceptionThrown && !cmd_allowDisconnect) {
            ARRAS_LOG_ERROR("An ARRAS exception was thrown during the run");
            session.exitCode = ERROR_EXIT_CODE;
        }

        if (session.arrasStopped) {
            ARRAS_LOG_WARN("The session stopped during the run");
            session.exitCode = ERROR_EXIT_CODE;
        }

        if (!session.receivedSessionStatus && !cmd_allowDisconnect) {
            ARRAS_LOG_ERROR("Didn't receive a session exit status");
            session.exitCode = ERROR_EXIT_CODE;
        }

        if (session.receivedSessionStatus && !subset(normalSessionStatus, session.sessionStatus)) {
            ARRAS_LOG_ERROR("Abnormal session status %s", session.sessionStatus.c_str());
            session.exitCode = ERROR_EXIT_CODE;
        }

        if ((session.exitCode != 0) && !ignoreErrors) return;
    }
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

    if (cmdOpts.count("help")) {
        std::cout << flags << std::endl;
        return 0;
    }

    for (auto i = 0; i < 16; i++) {
        computationoptions[i][arras4::api::MessageOptions::routingName] = std::string("For_benchcomp") + std::to_string(i);
    }

    arras4::log::Logger::Level logLevel;
    auto ll = cmdOpts["log-level"].as<unsigned short>();
    unsigned int repeat = cmdOpts["repeat"].as<unsigned>();
    unsigned int parallelism = cmdOpts["sessions"].as<unsigned>();

    bandwidthPath = cmdOpts["bandwidthPath"].as<std::string>();
    if (bandwidthPath == "computations_to_computations") {
        sessionName = "benchmark_many_test";
    } else {
        sessionName = cmdOpts["session"].as<std::string>();
    }
    stayConnectedSecs = cmdOpts["stay-connected"].as<unsigned>();
    dataSize = 0;
    dataSize += cmdOpts["mb"].as<unsigned>() * 1024 * (size_t)1024;
    dataSize += cmdOpts["bytes"].as<unsigned>();
    interval = cmdOpts["interval"].as<unsigned>();
    credits = cmdOpts["credits"].as<unsigned>();
    duration = cmdOpts["duration"].as<unsigned>();
    reportFrequency= cmdOpts["report-frequency"].as<unsigned>();
    logCount = cmdOpts["logCount"].as<unsigned>();
    printEnv = cmdOpts.count("printEnv");
    cmd_cores = 0.0;
    if (cmdOpts.count("cores")) {
        cmd_cores = cmdOpts["cores"].as<unsigned>();
    }
    cmd_delayStart = cmdOpts["delay-start"].as<unsigned>();
    cmd_requestMb = cmdOpts["requestMb"].as<unsigned>();
    cmd_threads = cmdOpts["threads"].as<unsigned>();
    cmd_allocateMb = cmdOpts["allocateMb"].as<unsigned>();
    cmd_touchMb =cmdOpts["touchMb"].as<unsigned>();
    cmd_touchOnce = cmdOpts["touchOnce"].as<unsigned>();
    cmd_logThreads = cmdOpts["logThreads"].as<unsigned>();
    cmd_logCount = cmdOpts["logCount"].as<unsigned>();
    cmd_messageSleep = cmdOpts["messageSleep"].as<unsigned>();
    cmd_noTimeout = cmdOpts.count("noTimeout");
    cmd_allowDisconnect = cmdOpts.count("allowDisconnect");
    cmd_localOnly = cmdOpts.count("local-only");
    cmd_phasedStart = cmdOpts["phased-start"].as<unsigned>();
    if (cmdOpts.count("prepend")) {
        cmd_prepend = cmdOpts["prepend"].as<std::string>();
    }
    if (cmdOpts.count("packaging-system")) {
        cmd_packaging_system= cmdOpts["packaging-system"].as<std::string>();
    }
    if (cmdOpts.count("host")) {
        cmd_host = cmdOpts["host"].as<std::string>();
    }
    cmd_port = cmdOpts["port"].as<unsigned short>();
    cmd_dc = cmdOpts["dc"].as<std::string>();
    cmd_env = cmdOpts["env"].as<std::string>();
    cmd_disconnectImmediately = cmdOpts.count("disconnectImmediately");

    if (ll <= arras4::log::Logger::LOG_TRACE) {
        logLevel = static_cast<arras4::log::Logger::Level>(ll);
    } else {
        std::cerr << "Supported log levels are 0-5" << std::endl;
        return 1;
    }  
    arras4::log::Logger::instance().setThreshold(logLevel);
    arras4::log::Logger::instance().setProcessName("client");
    arras4::log::Logger::instance().setThreadName("main");

    if (parallelism < 1) {
        ARRAS_LOG_ERROR("Illegal value for --sessions. Must be 1 or greater");
        exit(1);
    }

    sessions = new SessionInstance[parallelism];
    for (auto i = 0u; i < parallelism; i++) {
        sessions[i].mInstanceIndex = i;
        sessions[i].pSdk = std::make_shared<arras4::sdk::SDK>();
        initChunking(*sessions[i].pSdk, cmdOpts);
        sessions[i].pSdk->setMessageHandler(std::bind(&SessionInstance::messageHandler, &sessions[i], std::placeholders::_1));
        sessions[i].pSdk->setStatusHandler(std::bind(&SessionInstance::statusHandler, &sessions[i], std::placeholders::_1));
        sessions[i].pSdk->setExceptionCallback(std::bind(&SessionInstance::exceptionCallback, &sessions[i], std::placeholders::_1));
    }

    bool ignoreErrors = cmdOpts.count("ignore-errors");

    if (parallelism == 1) {
        sessionThread(0, repeat, ignoreErrors);
    } else {
        for (auto i = 0u; i < parallelism; i++) {
            if (cmd_phasedStart > 0) sleep(cmd_phasedStart);
            sessions[i].mThread = std::thread(&sessionThread, i, repeat, ignoreErrors);
        }
        for (auto i = 0u; i < parallelism; i++) {
            sessions[i].mThread.join();
        }
        for (auto i = 0u; i < parallelism; i++) {
            if (sessions[i].exitCode != 0) return sessions[i].exitCode;
        }
    }

    delete [] sessions;

    return 0;
}
