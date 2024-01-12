// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <atomic>
#include <cmath> // round
#include <cstdlib> // std::getenv
#include <functional> // bind
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h> // sleep
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <dirent.h>
#include <sys/types.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include <json/json.h>

#include <message_api/Message.h>
#include <message_api/ArrasTime.h>
#include <client/api/AcapAPI.h>
#include <client/api/SessionDefinition.h>

#include <network/DataSink.h>

#include <arras4_log/Logger.h>

#include <sdk/sdk.h>

namespace bpo = boost::program_options;

constexpr size_t READBUF_SIZE = 16*1024;
char READBUF[READBUF_SIZE];

const std::string MSG_EXT(".msg");

arras4::api::ArrasTime DELAY(1,0);
const int MAX_DELAY = 30; // maximum delay in seconds

unsigned short constexpr DEFAULT_CON_WAIT_SECS = 30;
unsigned short constexpr DEFAULT_LOG_LEVEL = 2;
unsigned short constexpr DEFAULT_ACAP_PORT = 8087;

const std::string DEFAULT_ENV_NAME = "prod";
const std::string DEFAULT_ACAP_PATH = "/coordinator/1/sessions";

std::atomic<bool> arrasStopped(false);
std::atomic<bool> arrasExceptionThrown(false);

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
        ("log-level,l", bpo::value<unsigned short>()->default_value(DEFAULT_LOG_LEVEL), "Log level [0-5] with 5 being the highest")
        ("athena-env",bpo::value<std::string>()->default_value("prod"),"Environment for Athena logging")
        ("trace-level",bpo::value<int>()->default_value(0),"trace threshold level (-1=none,5=max)")

        ("sessionFile",bpo::value<std::string>(), "Session file to load")
        ("path", bpo::value<std::string>()->default_value("."), "Path to message directory")
        ("timestamps","use filename timestamps to determine delay between messages")
    ;

    bpo::positional_options_description positionals;
    positionals.add("sessionFile", 1);

    bpo::store(bpo::command_line_parser(argc, argv).
               options(flags).positional(positionals).run(), cmdOpts);
    bpo::notify(cmdOpts);
}

std::string
getArrasUrl(arras4::sdk::SDK& sdk, const bpo::variables_map& cmdOpts)
{
    std::string url;
    if (cmdOpts.count("host")) {
        std::ostringstream ss;
        ss << "http://" << cmdOpts["host"].as<std::string>()
           << ":" << cmdOpts["port"].as<unsigned short>()
           << DEFAULT_ACAP_PATH;

        url = ss.str();
    } else {
        url = sdk.requestArrasUrl(cmdOpts["dc"].as<std::string>(), cmdOpts["env"].as<std::string>());
        std::ostringstream msg;
        msg << "Received " << url << " from Studio Config Service.";
        ARRAS_LOG_DEBUG(msg.str());
    }

    return url;
}

bool
connect(arras4::sdk::SDK& sdk,
        const std::string& sessionFile,
        const bpo::variables_map& cmdOpts)
{
    try {
        arras4::client::SessionOptions so;

        arras4::client::SessionDefinition def;
        def.loadFromFile(sessionFile);
        const std::string& arrasUrl = getArrasUrl(sdk, cmdOpts);
        const std::string& response = sdk.createSession(def, arrasUrl, so);
        if (response.empty()) {
            ARRAS_LOG_ERROR("Failed to connect to Arras service: %s", arrasUrl.c_str());
            return false;
        }

        std::cout << "Created session id " << response << std::endl;

    } catch (arras4::sdk::SDKException& e) {
        ARRAS_LOG_ERROR("Unable to connect to Arras: %s", e.what());
        return false;
    } catch (arras4::client::DefinitionLoadError& e) {
        ARRAS_LOG_ERROR("Failed to load session: %s", e.what());
        return false;
    }
    return true;
}



void
messageHandler(const arras4::api::Message& msg)
{
    std::cout << "Received: " << msg.describe() << std::endl;
}

void
statusHandler(const std::string& status)
{
    // Check to see if the new status is a json doc
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(status, root)) {
        Json::Value execStatus = root.get("execStatus", Json::Value());
        if (execStatus.isString() && 
            (execStatus.asString() == "stopped" || 
             execStatus.asString() == "stopping")) {
            
            arrasStopped = true;
            Json::Value stopReason = root.get("execStoppedReason", Json::Value());

            std::ostringstream msg;
            msg << "Arras session has stopped";

            if (stopReason.isString()) {
                msg << " due to: " << stopReason.asString();
            }

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
        ARRAS_LOG_INFO("Received status change to: %s", status.c_str());
    }
}

void
exceptionCallback(const std::exception& e)
{
    ARRAS_LOG_ERROR("Thrown exception: %s", e.what());
    arrasExceptionThrown = true;
}

bool playFile(arras4::network::FramedSink* sink,
              const std::string& filepath)
{
    // get file size for framing
    struct stat64 stat_buf;
    int rc = stat64(filepath.c_str(), &stat_buf);
    if (rc != 0) {
        std::cerr << "failed to stat file " << filepath << std::endl;
        return false;
    }
    
    size_t size = stat_buf.st_size;
    sink->openFrame(size);

    std::ifstream ifs(filepath);
    if (!ifs) {
        std::cerr << "failed to open message file " << filepath << std::endl;
        return false;
    }
    while (ifs) {
        ifs.read(READBUF,READBUF_SIZE);
        if (ifs.bad()) break;
        sink->write(reinterpret_cast<unsigned char*>(READBUF),ifs.gcount());
    }
    if (ifs.bad()) {
        std::cerr << "failed to read from message file " << filepath << std::endl;
        return false;
    }
    sink->closeFrame();
    ifs.close();
    return true;
}   

bool getFilenames(const std::string& dir,
                  std::vector<std::string>& filenames)
{
    DIR *dp = opendir(dir.c_str());
    if (dp == nullptr) {
        std::cerr << "failed to read directory " << dir << std::endl;
        return false;
    }
  
    struct dirent *dirp;
    while ((dirp = readdir(dp)) != nullptr) {
        std::string fname(dirp->d_name);
        if ((fname.size() > 4) &&
            std::equal(MSG_EXT.rbegin(), MSG_EXT.rend(),fname.rbegin())) {
            filenames.push_back(fname);
        }
    }
    closedir(dp);
    if (filenames.size() == 0) {
        std::cerr << "no message files found in directory " << dir << std::endl;
        return false;
    }
    std::sort(filenames.begin(),filenames.end());
    return true;
}

void doDelay(const arras4::api::ArrasTime& t)
{
    int secs = t.seconds;
    if (secs > MAX_DELAY) 
        secs = MAX_DELAY;
    long ms = secs*1000L + (t.microseconds/1000);
    if (ms > 0) {
        std::cout << "(delay " << (((double)ms)/1000) << " seconds)" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

bool playMessages(arras4::network::FramedSink* sink, 
                  const std::string& path,
                  bool useTimestamps)
{
    std::vector<std::string> filenames;
    bool ok = getFilenames(path,filenames);
    if (!ok)
        return false;
   
    arras4::api::ArrasTime curr;
    arras4::api::ArrasTime prev;
    arras4::api::ArrasTime delay;
    for (std::string filename : filenames) {
        if (useTimestamps) {
            ok = curr.fromFilename(filename);
            if (ok && (prev != arras4::api::ArrasTime::zero)) {
                delay = curr - prev;
            } else {
                delay = DELAY;
            }
            if (ok)
                prev = curr;
            else
                prev = prev + delay; 
        } else {
            delay = DELAY;
        }
        doDelay(delay);
        std::cout << "Playing " << filename << std::endl;
        std::string fullpath = path + "/" + filename;
        ok = playFile(sink,fullpath);
        if (!ok) {
            return false;
        }
    }
    return true;
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

    arras4::sdk::SDK::configAthenaLogger(cmdOpts["athena-env"].as<std::string>());

    arras4::log::Logger::Level logLevel;
    auto ll = cmdOpts["log-level"].as<unsigned short>();
    if (ll <= arras4::log::Logger::LOG_TRACE) {
        logLevel = static_cast<arras4::log::Logger::Level>(ll);
    } else {
        std::cerr << "Supported log levels are 0-5" << std::endl;
        return 1;
    }
    arras4::log::Logger::instance().setThreshold(logLevel);
    arras4::log::Logger::Level traceLevel = static_cast<arras4::log::Logger::Level>(cmdOpts["trace-level"].as<int>());
    arras4::log::Logger::instance().setTraceThreshold(traceLevel);

 
    std::shared_ptr<arras4::sdk::SDK> pSdk = std::make_shared<arras4::sdk::SDK>();
    pSdk->setMessageHandler(&messageHandler);
    pSdk->setStatusHandler(&statusHandler);
    pSdk->setExceptionCallback(&exceptionCallback);
    
    int exitStatus = 0;    

    std::string sessionFile;
    if (cmdOpts.count("sessionFile")) {
        sessionFile = cmdOpts["sessionFile"].as<std::string>();
    } else {
        std::cerr << "Must specify session file" << std::endl;
        return 1;
    }
   
      
    if (!connect(*pSdk, sessionFile, cmdOpts)) {
        std::cerr << "Failed to connect!" << std::endl;
        return 1;
     }
   
     std::cout << "Waiting for ready signal..." << std::endl;
     bool ready = pSdk->waitForEngineReady(DEFAULT_CON_WAIT_SECS);
   

    if (!pSdk->isConnected() || !ready || arrasStopped) {
        std::cerr << "Failed to connect!" << std::endl;
        return 1;
    }
 
   
    // this is a bit of a trick : we don't want to use the SDK send function
    // directly, because that would require us to deserialize the message from the
    // message file, only for it to be immediately reserialized by MessageWriter for
    // transport over the socket. One way around this is to use the OpaqueContent
    // class from message_impl, since this is specifically designed to represent
    // non-deserialized message content. However, it is even easier to process the
    // file as a data frame rather than a message, and send it direct to the
    // outgoing framed sink. This is possible because PeerMessageEndpoint
    // has a backdoor...
    arras4::network::FramedSink* sink = pSdk->endpoint()->framedSink();
    
    bool useTimestamps = cmdOpts.count("timestamps") > 0;
    bool ok = playMessages(sink,cmdOpts["path"].as<std::string>(),useTimestamps);
    if (!ok) {
        return 1;
    }

    // wait for disconnect
    std::cout << "Waiting for disconnect" << std::endl;
    while(pSdk->isConnected() && !arrasExceptionThrown && !arrasStopped) {
            sleep(1);
    }

    if (arrasExceptionThrown || arrasStopped) {
        exitStatus = 1;
    }

    return exitStatus;
}

