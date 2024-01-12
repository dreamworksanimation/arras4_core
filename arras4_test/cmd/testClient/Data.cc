// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <http/HttpResponse.h>
#include <http/HttpRequest.h>
#include <json/reader.h>
#include <string>

#include "Data.h"

using namespace arras4::network;

const char* DWA_CONFIG_ENV_NAME = "DWA_CONFIG_SERVICE";
const char* ARRAS_CONFIG_PATH = "serve/jose/arras/endpoints/";

int
getResourcesFromConfigurationService(Json::Value& root, const std::string& datacenter, const std::string& environment)
{
        // Get dwa studio configuration service url from env
    const char* configSrvUrl = std::getenv(DWA_CONFIG_ENV_NAME);
    if (configSrvUrl == nullptr) {
        std::cerr << "Undefined environment variable: " << DWA_CONFIG_ENV_NAME;
        exit(1);
    }

    // Build the resource url
    std::string url(configSrvUrl);
    url += ARRAS_CONFIG_PATH;
    url += datacenter;
    url += "/";
    url += environment;

    return getJSON(url, root);
}

int
initServiceUrls(const std::string& datacenter, const std::string& environment,
                std::string& coordinator, std::string& logs, std::string& consul)
{
    Json::Value resources;
    if (getResourcesFromConfigurationService(resources, datacenter, environment)) {
        return 1;
    }
    if (resources["coordinator"]["url"].isString()) {
        coordinator = resources["coordinator"]["url"].asString();
    } else {
        return 1;
    }
    if (resources["arraslogs"]["url"].isString()) {
        logs = resources["arraslogs"]["url"].asString();
    } else {
        return 1;
    }
    if (resources["consul"]["url"].isString()) {
        consul = resources["consul"]["url"].asString();
    } else {
        return 1;
    }
    return 0;
}

int
getJSON(const std::string& url, Json::Value& root)
{
    // fprintf(stderr,"JSON url =  %s\n", url.c_str());
    HttpRequest req(url, GET);
    req.setUserAgent("");
    const HttpResponse& resp = req.submit();

    if (resp.responseCode() != HTTP_OK) return 1;

    std::string responseString;
    if (!resp.getResponseString(responseString)) {
        return 1;
    }

    // printf("JSON = %s\n", responseString.c_str());


    Json::Reader reader;
    if (reader.parse(responseString, root)) {
    } else {
        std::cerr << "Failed to parse json\n";
        return 1;
    }

    return 0;
}

void
getLogs(
    const std::string& logger,
    std::vector<Session>& sessions,
    unsigned int logLines)
{
    for (auto iter=sessions.begin(); iter != sessions.end(); ++iter) {
        Session& session = *iter;
        std::string queryUrl = logger + "/logs/session/" + session.mId + "?sort=desc&page=0&size=" + std::to_string(logLines);

        Json::Value jsonLogInfo;
        try {
            getJSON(queryUrl, jsonLogInfo);
        } catch (...) {
            session.mLogLines.push_back("Error getting logs");
            continue;
        }

        Json::Value jsonContent = jsonLogInfo["content"];
        if (jsonContent.isArray()) {
            Json::ArrayIndex size = jsonContent.size();
            if (size == 0) {
                session.mLogLines.push_back("No logs found");
            } else {
                for (int i = size-1; i >= 0; i--) {
                    // this shouldn't really get errors but ignore it if it does
                    if (!jsonContent[i]["output"].isString()) continue;
                    if (!jsonContent[i]["timestamp"].isString()) continue;

/*
   {
      "id": "arras_logs-2019.03.14+4+218655807",
      "sessionId": "89b66efc-8130-0d92-b88a-a81561e2e85e",
      "output": ": UPDATING SESSION STATUS TO STOPPING, due to clientShutdown",
      "loglevel": "D",
      "processname": "node",
      "pid": "47752",
      "thread": "140737139648256",
      "timestamp": "2019-03-14T15:38:57.321576-07:00",
      "hostname": "tealstar.anim.dreamworks.com",
      "timestampEpochUsecs": "1552603137321576"
    }
*/

        
                    std::string line(jsonContent[i]["hostname"].asString() +
                                     ": " + jsonContent[i]["timestamp"].asString() +
                                     " " + jsonContent[i]["loglevel"].asString() +
                                     " " + jsonContent[i]["processname"].asString() +
                                     "[" + jsonContent[i]["pid"].asString() + "]:" +
                                     jsonContent[i]["thread"].asString() +
                                     " [" + jsonContent[i]["sessionId"].asString() + "]" +
                                     jsonContent[i]["output"].asString());
                    session.mLogLines.push_back(line);
                }
            }
        }
    }
}

int
getSessions(
    const std::string& coordinator,
    const std::string& user,
    const std::vector<std::string>& sessionFilter,
    std::map<std::string,Node>& nodes,
    std::map<std::string,Session>& sessions)
{
    Json::Value jsonSessions;

    sessions.clear();
    nodes.clear();

    // build a url for a request of all of the current sessions
    std::string queryUrl = coordinator + "/sessions";
    getJSON(queryUrl, jsonSessions);

    if (!jsonSessions.isArray()) return 1;
    Json::ArrayIndex size = jsonSessions.size();


    for (auto i = 0; i < size; i++) {
        Json::Value value = jsonSessions[i];
        if (value.isObject()) {
            Session session;
            session.mId =  value["id"].asString();

            // filter sessions if a list is given
            if (sessionFilter.size() > 0) {
                bool hit = false;
                for (auto filterIter = sessionFilter.begin(); filterIter != sessionFilter.end(); ++filterIter) {
                    if (*filterIter == session.mId.substr(0, filterIter->length())) {
                        hit = true;
                        break;
                    }
                }
                if (!hit) continue; // skip this one
            }

            session.mClientUser =  value["clientInfo"]["user"]["name"].asString();

            // skip other users if a user is specified
            if ((user != "") && (session.mClientUser != user)) continue;

            session.mEntryNodeId = value["entryNodeId"].asString();
            Json::Value jsonNodes = value["nodes"];
            // printf("    node\n");
            for (auto iter = jsonNodes.begin(); iter != jsonNodes.end(); ++iter) {
                Json::ValueIterator::reference nodeJson = *iter;
                Node node;
                node.mId = nodeJson["id"].asString();
                node.mHostname = nodeJson["hostname"].asString();
                node.mIpAddress = nodeJson["ipAddress"].asString();
                node.mHttpPort = nodeJson["httpPort"].asUInt();
                node.mPort = nodeJson["port"].asUInt();
                std::string status = nodeJson["status"].asString();
                if (status == "UP") {
                    node.mStatus = Node::UP;
                } else {
                    node.mStatus = Node::NOTUP;
                }
                if (nodeJson["exclusiveUser"].isString()) {
                    node.mExclusiveUser = nodeJson["exclusiveUser"].asString();
                } else {
                    node.mExclusiveUser = "NONE";
                }
                node.mOverSubscribe = nodeJson["overSubscribe"].asBool();
                node.mCores = nodeJson["resources"]["cores"].asFloat();
                node.mMemoryMB = nodeJson["resources"]["memoryMB"].asFloat();
                node.mStatusUrl = std::string("http://") +
                                   node.mIpAddress + ":" + std::to_string(node.mHttpPort) +
                                   "/node/1/sessions";
                
                // this will get written multiple times but it should always be with the same data
                nodes[node.mId] = node;
            }

            Json::Value assignments = value["assignments"];
            // printf("    assignments\n");
            for (auto iter = assignments.begin(); iter != assignments.end(); ++iter) {
                Json::ValueIterator::reference assignmentJson = *iter;
                std::string nodeId = assignmentJson["nodeId"].asString();
                // most of the node information is redundant
                const Json::Value computations = assignmentJson["config"]["computations"];
                for (Json::Value::const_iterator iter = computations.begin(); iter != computations.end(); ++iter) {
                    Json::ValueConstIterator::reference computationJson = *iter;
                    Computation computation;
                    computation.mId = iter.key().asString();
                    if (computation.mId == "(client)") continue;
                    computation.mName = computationJson["name"].asString();
                    computation.mDso = computationJson["dso"].asString();
                    if (computationJson["requirements"]["computationAPI"].isString()) {
                        computation.mComputationAPI = computationJson["requirements"]["computationAPI"].asString();
                    } else {
                        computation.mComputationAPI = "none specified";
                    }
                    if (computationJson["requirements"]["rez_packages"].isString()) {
                        computation.mRezPackages= computationJson["requirements"]["rez_packages"].asString();
                    } else {
                        computation.mRezPackages = "none specified";
                    }
                    computation.mNodeId = nodeId;
                    computation.mStats.mReservedCores = computationJson["requirements"]["resources"]["cores"].asFloat();
                    computation.mStats.mReservedMemory = computationJson["requirements"]["resources"]["memoryMB"].asFloat() * 1048576;
                    session.mComputations[computation.mName] = computation;
                }
                sessions[session.mId] = session;
            }
        }
    }

    // returns number of sessions,
    return static_cast<int>(sessions.size());
}

void
aggregateComputationStats(std::map<std::string,Session>& sessions)
{
    for (auto sessIter = sessions.begin(); sessIter != sessions.end(); ++sessIter) {
        Session& session = sessIter->second;

        for (auto compIter = session.mComputations.begin(); compIter != session.mComputations.end(); ++compIter) {
            Computation& computation = compIter->second;

            session.mCompStats = session.mCompStats + computation.mStats;
        }
    }
}

void
getComputationDetails(std::map<std::string,Node>& nodes, std::map<std::string,Session>& sessions)
{
    // for each session walk through the list of computations and for
    // for each computation which doesn't have the details filled in,
    // request details from the node. The node response will include
    // answers for all of the computations on that node
    for (auto sessIter = sessions.begin(); sessIter != sessions.end(); ++sessIter) {
        Session& session = sessIter->second;

        // check that every computation has it's detailed status
        for (auto compIter = session.mComputations.begin(); compIter != session.mComputations.end(); ++compIter) {
            Computation& computation = compIter->second;
            if (!computation.mHasStatus) {

                // if it doesn't have it's status then query the node for that computation
                auto nodeIter = nodes.find(computation.mNodeId);
                if (nodeIter != nodes.end()) {
                    Node& node = nodeIter->second;
                    Json::Value sessionStatus;
                    std::string queryUrl = node.mStatusUrl + "/" + session.mId + "/status";
                    try {
                        if (getJSON(queryUrl, sessionStatus)) {
                            fprintf(stderr, "couldn't get computation status\n");
                        }
                    } catch (...) {
                        node.mDefunct = true;
                        computation.mDefunct = true;
                        session.mHasDefunct = true;
                        continue;
                    }
                    session.mHasNonDefunct = true;

                    // there will be a list of computations on that node so it
                    // may fill in status for other computations too
                    Json::Value& computations = sessionStatus["computations"];
                    if (computations.isArray()) {
                        Json::ArrayIndex size = computations.size();
                        for (auto i = 0; i < size; i++) {
                            Json::Value jsonStatus = computations[i];
                            std::string name = jsonStatus["name"].asString();
                            auto iter = session.mComputations.find(name);
                            if (iter == session.mComputations.end()) {
                                printf("Didn't find %s\n", name.c_str());
                            } else {
                                Computation& comp = iter->second;

                                // skip this if data is already filled in
                                if (comp.mHasStatus) continue;

                                ComputationStats& stats = comp.mStats;

                                stats.mLastHeartbeatTime = jsonStatus["lastHeartbeatTime"].asString();
                                // skip this if there aren't valid stats for this computation
                                if (stats.mLastHeartbeatTime.empty()) continue;

                                stats.mCpuUsage5Secs = jsonStatus["cpuUsage5Secs"].asFloat();
                                stats.mCpuUsage5SecsMax = jsonStatus["cpuUsage5SecsMax"].asFloat(); 
                                stats.mCpuUsage60Secs = jsonStatus["cpuUsage60Secs"].asFloat();
                                stats.mCpuUsage60SecsMax = jsonStatus["cpuUsage60SecsMax"].asFloat();
                                stats.mCpuUsageTotalSecs = jsonStatus["cpuUsageTotalSecs"].asFloat();
                                stats.mMemoryUsageBytes = jsonStatus["memoryUsageBytesCurrent"].asUInt64();
                                stats.mMemoryUsageBytesMax = jsonStatus["memoryUsageBytesMax"].asUInt64();
                                stats.mSentMessagesCount5Secs = jsonStatus["sentMessagesCount5Secs"].asUInt();
                                stats.mSentMessagesCount60Secs = jsonStatus["sentMessagesCount60Secs"].asUInt();
                                stats.mSentMessagesCountTotal = jsonStatus["sentMessagesCountTotal"].asUInt();
                                stats.mReceivedMessagesCount5Secs = jsonStatus["receivedMessagesCount5Secs"].asUInt();
                                stats.mReceivedMessagesCount60Secs = jsonStatus["receivedMessagesCount60Secs"].asUInt();
                                stats.mReceivedMessagesCountTotal = jsonStatus["receivedMessagesCountTotal"].asUInt();
                                stats.mLastSentMessageTime = jsonStatus["lastSentMessagesTime"].asString();
                                stats.mLastReceivedMessageTime = jsonStatus["lastReceivedMessagesTime"].asString();
                                stats.mLastHeartbeatTime = jsonStatus["lastHeartbeatTime"].asString();

                                comp.mSignal = jsonStatus["signal"].asString();
                                comp.mStoppedReason = jsonStatus["stoppedReason"].asString();
                                comp.mExecStatus = jsonStatus["execStatus"].asString();
                                comp.mCompStatus = jsonStatus["compStatus"].asString();
                                comp.mHasStatus = true;
                            }
                        }
                    }
                }
            }
        }
    }
}

