// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <computation_api/Computation.h>
#include <computation_impl/CompEnvironmentImpl.h>
#include <message_impl/PeerMessageEndpoint.h>

#include <network/IPCSocketPeer.h>

#include <json/value.h>
#include <json/reader.h>

#include <iostream>
#include <unistd.h>
#include <fstream>
#include <exception>
#include <signal.h>

using namespace arras4::network;
using namespace arras4::api;
using namespace arras4::impl;

Peer* createServerPeer(const std::string& ipcAddr)
{
    std::cout << "Waiting for connection at " << ipcAddr << std::endl;
    unlink(ipcAddr.c_str());
    IPCSocketPeer listenPeer;
    listenPeer.listen(ipcAddr);
    Peer* peer = nullptr;
    Peer** peers = &peer;
    while (true) { 
        int count = 1;        
        listenPeer.accept(peers,count,1000);
        if (count > 0) break;
    }
    std::cout << "Connection accepted" << std::endl;
    return peer;
}

bool loadConfig(const std::string& path,
                Json::Value& out)
{
    std::ifstream ifs(path);
    if (ifs.fail()) {
        std::cerr << "Failed to open config file '" 
                  << path << "'" << std::endl;
        return false;
    }
    try {
        ifs >> out;
        return true;
    } catch (std::exception& e) {
        std::cerr << "Error reading config file '" << path 
                  << "': " << e.what() << std::endl;
        return false;
    }
}

int usage() 
{
    std::cout << "Args: configfile ipcaddr" << std::endl;
    return -1;
}

int main(int argc,char** argv)
{      
    if (argc != 3)
        return usage();
   
    Json::Value config;
    if (!loadConfig(argv[1],config)) {
         return -1;
    }

    if (!config["config"].isObject()) {
         std::cerr << "Error: config file contain 'config' object" << std::endl;
         return -1;
    }
    Json::Value& compConfig = config["config"];
  
    ExecutionLimits limits;
   
    if (config["limits"].isObject()) {
        limits.setFromObject(config["limits"]);
    }

    if (compConfig["dso"].isNull()) {
        std::cerr << "Error: config file must specify dso name" << std::endl;
        return -1;
    }

    std::string dsoName = compConfig["dso"].asString();
    bool waitForGo = false;
    if (config["waitForGo"].isBool())
        waitForGo = config["waitForGo"].asBool();


    try {
        std::cout << "Initalizing computation..." << dsoName << std::endl;
        CompEnvironmentImpl env(dsoName,dsoName,Address());
      
        Result res = env.initializeComputation(limits,compConfig);
        if (res == Result::Invalid) {
            std::cout << "Initialization FAILED" << std::endl;
            return -1;
        }

        Peer* peer = createServerPeer(argv[2]);
        PeerMessageEndpoint endpoint(*peer,true,"none none");
        
        std::cout << "Starting computation..." << std::endl;
        ComputationExitReason er = env.runComputation(endpoint,limits,waitForGo);

        std::cout << "Environment exited : " 
                  << computationExitReasonAsString(er) << std::endl;
    } catch (std::exception& e) {
        std::cerr <<  "Error running computation " << dsoName << ": " 
                  << e.what() << std::endl;
        return -1;
    }
    return 0;
}

