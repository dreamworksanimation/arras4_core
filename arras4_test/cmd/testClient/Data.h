// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef _DATA_INCLUDED_
#define _DATA_INCLUDED_

#include <json/json.h>
#include <limits>

// reportable data
enum ColumnType {
    FULL_ID,
    SHORT_ID,
    COMP_NAME,
    COMP_STATUS,
    NODE,
    EXEC_STATUS,
    STOPPED_REASON,
    SIGNAL,
    CPU_USAGE_5,
    CPU_USAGE_5MAX,
    CPU_USAGE_60,
    CPU_USAGE_60MAX,
    CPU_USAGE_TOTAL,
    SENT_MESSAGES_5,
    SENT_MESSAGES_60,
    SENT_MESSAGES_TOTAL,
    SENT_MESSAGE_TIME,
    RECEIVED_MESSAGES_5,
    RECEIVED_MESSAGES_60,
    RECEIVED_MESSAGES_TOTAL,
    RECEIVED_MESSAGE_TIME,
    HEARTBEAT_TIME,
    MEMORY,
    MEMORY_MAX,
    RESERVED_CORES,
    RESERVED_MEMORY,
    // computation details

    // session details
    SESSION_CLIENT_USER,
    SESSION_ENTRY_NODE,

    COLUMNTYPE_INVALID
};

class Node {
  public:
    Node() : mStatus(NOTUP), 
             mHttpPort(0), 
             mPort(0), 
             mOverSubscribe(false),
             mCores(0.f),
             mMemoryMB(0.f),
             mDefunct(false) {}
    enum NodeStatus {
        UP,
        NOTUP
    };
    std::string mId;
    std::string mHostname;
    std::string mExclusiveUser;
    std::string mStatusUrl;
    std::string mIpAddress;
    NodeStatus mStatus;
    unsigned short mHttpPort;
    unsigned short mPort;
    bool mOverSubscribe;
    float mCores;
    float mMemoryMB;
    bool mDefunct;
};

//
// ComputationStats is the per computation information which can be reasonably
// summed or summerized across all of the computations
//
class ComputationStats {
  public:
    ComputationStats() :
        mCpuUsage5Secs(std::numeric_limits<float>::quiet_NaN()),
        mCpuUsage5SecsMax(std::numeric_limits<float>::quiet_NaN()),
        mCpuUsage60Secs(std::numeric_limits<float>::quiet_NaN()),
        mCpuUsage60SecsMax(std::numeric_limits<float>::quiet_NaN()),
        mCpuUsageTotalSecs(std::numeric_limits<float>::quiet_NaN()),
        mReservedCores(std::numeric_limits<float>::quiet_NaN()),
        mMemoryUsageBytes(-1),
        mMemoryUsageBytesMax(-1),
        mSentMessagesCount5Secs(-1),
        mSentMessagesCount60Secs(-1),
        mSentMessagesCountTotal(-1),
        mReceivedMessagesCount5Secs(-1),
        mReceivedMessagesCount60Secs(-1),
        mReceivedMessagesCountTotal(-1),
        mReservedMemory(-1) {}
    void zero() {
        mCpuUsage5Secs=0.0;
        mCpuUsage5SecsMax=0.0;
        mCpuUsage60Secs=0.0;
        mCpuUsage60SecsMax=0.0;
        mCpuUsageTotalSecs=0.0;
        mReservedCores=0.0;
        mMemoryUsageBytes=0;
        mMemoryUsageBytesMax=0;
        mSentMessagesCount5Secs=0;
        mSentMessagesCount60Secs=0;
        mSentMessagesCountTotal=0;
        mReceivedMessagesCount5Secs=0;
        mReceivedMessagesCount60Secs=0;
        mReceivedMessagesCountTotal=0;
        mReservedMemory=0;
        mLastSentMessageTime.clear();
        mLastReceivedMessageTime.clear();
        mLastHeartbeatTime.clear();
    }

    static long add(const long a, const long b) {
        if ((a < 0) || (b < 0)) {
            return -1;
        } else {
            return a + b;
        }
    }
    static std::string add(const std::string& a, const std::string& b) {
        if (a == "") {
            return b;
        } else if (b == "") {
            return a;
        } else if (a > b) {
            return a;
        } else {
            return b;
        }
    }


    // do a special add that considers negatives invalid and makes invalid
    // contagious like NaN. Dates choose the most recent.
    ComputationStats operator + (const ComputationStats& a) const {
        ComputationStats x;
        x.mCpuUsage5Secs=     mCpuUsage5Secs +     a.mCpuUsage5Secs;
        x.mCpuUsage5SecsMax=  mCpuUsage5SecsMax +  a.mCpuUsage5SecsMax;
        x.mCpuUsage60Secs=    mCpuUsage60Secs +    a.mCpuUsage60Secs;
        x.mCpuUsage60SecsMax= mCpuUsage60SecsMax + a.mCpuUsage60SecsMax;
        x.mReservedCores =    mReservedCores +     a.mReservedCores;
        x.mCpuUsageTotalSecs= mCpuUsageTotalSecs + a.mCpuUsageTotalSecs;
        x.mMemoryUsageBytes=  add(mMemoryUsageBytes, a.mMemoryUsageBytes);
        x.mMemoryUsageBytesMax=  add(mMemoryUsageBytesMax, a.mMemoryUsageBytesMax);
        x.mSentMessagesCount5Secs=  add(mSentMessagesCount5Secs, a.mSentMessagesCount5Secs);
        x.mSentMessagesCount60Secs=  add(mSentMessagesCount60Secs, a.mSentMessagesCount60Secs);
        x.mSentMessagesCountTotal=  add(mSentMessagesCountTotal, a.mSentMessagesCountTotal);
        x.mReceivedMessagesCount5Secs=  add(mReceivedMessagesCount5Secs, a.mReceivedMessagesCount5Secs);
        x.mReceivedMessagesCount60Secs=  add(mReceivedMessagesCount60Secs, a.mReceivedMessagesCount60Secs);
        x.mReceivedMessagesCountTotal=  add(mReceivedMessagesCountTotal, a.mReceivedMessagesCountTotal);
        x.mReservedMemory =             add(mReservedMemory , a.mReservedMemory);           
        x.mLastSentMessageTime= add(mLastSentMessageTime, a.mLastSentMessageTime);
        x.mLastReceivedMessageTime= add(mLastReceivedMessageTime, a.mLastReceivedMessageTime);
        x.mLastHeartbeatTime= add(mLastHeartbeatTime, a.mLastHeartbeatTime);
        return x;
    }
    float mCpuUsage5Secs;
    float mCpuUsage5SecsMax;
    float mCpuUsage60Secs;
    float mCpuUsage60SecsMax;
    float mCpuUsageTotalSecs;
    float mReservedCores;
    long mMemoryUsageBytes;
    long mMemoryUsageBytesMax;
    long mSentMessagesCount5Secs;
    long mSentMessagesCount60Secs;
    long mSentMessagesCountTotal;
    long mReceivedMessagesCount5Secs;
    long mReceivedMessagesCount60Secs;
    long mReceivedMessagesCountTotal;
    long mReservedMemory;
    std::string mLastSentMessageTime;
    std::string mLastReceivedMessageTime;
    std::string mLastHeartbeatTime;
};

class Computation {
  public:
    Computation() :
        mHasStatus(false),
        mDefunct(false) {}
    ComputationStats mStats;
    std::string mId;
    std::string mName;
    std::string mDso;
    std::string mNodeId;
    std::string mRezPackages;
    std::string mComputationAPI;
    bool mHasStatus;
    std::string mSignal;
    std::string mStoppedReason;
    std::string mExecStatus;
    std::string mCompStatus;
    bool mDefunct;
};

class Session {
  public:
    Session() : mHasDefunct(false), mHasNonDefunct(false) {
        mCompStats.zero();
    }
    std::string mId;
    std::string mEntryNodeId;
    std::string mClientUser;
    std::map<std::string,Computation> mComputations;
    ComputationStats mCompStats;
    bool mHasDefunct;
    bool mHasNonDefunct;
    std::vector<std::string> mLogLines;
};

int
initServiceUrls(const std::string& datacenter, const std::string& environment,
                std::string& coordinator, std::string& logs, std::string& consul);

int
getResourcesFromConfigurationService(Json::Value& root, const std::string& datacenter="gld", const std::string& environment="stb");



//
// Get the session information which is available from coordinator
//
int getSessions(
    const std::string& coordinator,                 // the base url of the coordinator
    const std::string& user,                        // a user to filter on
    const std::vector<std::string>& sessionFilter,  // sessionid to filter on
    std::map<std::string,Node>& nodes,              // a place to store collected nodes
    std::map<std::string,Session>& sessions);       // a place to store collected session

//
// Get the detailed computation available from the nodes
//
void getComputationDetails(
    std::map<std::string, Node>& nodes,             // a table of node details
    std::map<std::string,Session>& sessions);       // the sessions to get details on

//
// summarize the computation details into the sessions
//
void aggregateComputationStats(std::map<std::string,Session>& sessions);


int getJSON(const std::string& url, Json::Value& root);

//
// Get the log tail for the sessions
//
void
getLogs(
    const std::string& logger,                      // the base url of the logger
    std::vector<Session>& sessions,                 // the sessions to get logs for
    unsigned int logLines);                         // the number of log lines to get


#endif // _DATA_INCLUDED_

