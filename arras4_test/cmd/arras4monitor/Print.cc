// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <math.h>
#include <string>

#include "Data.h"
#include "Print.h"
#include "Spreadsheet.h"

Spreadsheet::Alignment
ChooseAlignment(ColumnType type)
{
    switch (type) {
      case CPU_USAGE_TOTAL:
      case CPU_USAGE_5:
      case CPU_USAGE_5MAX:
      case CPU_USAGE_60:
      case CPU_USAGE_60MAX:
      case RECEIVED_MESSAGES_5:
      case RECEIVED_MESSAGES_60:
      case RECEIVED_MESSAGES_TOTAL:
      case SENT_MESSAGES_5:
      case SENT_MESSAGES_60:
      case SENT_MESSAGES_TOTAL:
        return Spreadsheet::ALIGN_RIGHT;
      default:
        return Spreadsheet::ALIGN_LEFT;
    }
}

std::string
PrintHeading(ColumnType type)
{
    switch (type) {
      case FULL_ID:
      case SHORT_ID:
        return "ID";
      case COMP_NAME:
        return "NAME";
      case COMP_STATUS:
        return "COMP STATUS";
      case EXEC_STATUS:
        return "EXEC STATUS";
      case STOPPED_REASON:
        return "STOP REASON";
      case SIGNAL:
        return "SIGNAL";
      case CPU_USAGE_5:
        return "%CPU";
      case CPU_USAGE_5MAX:
        return "MAX%CPU";
      case CPU_USAGE_60:
        return "%CPU60";
      case CPU_USAGE_60MAX:
        return "MAX%CPU60";
      case CPU_USAGE_TOTAL:
        return "CPU_TIME";
      case SENT_MESSAGES_5:
        return "SENT5";
      case SENT_MESSAGES_60:
        return "SENT60";
      case SENT_MESSAGES_TOTAL:
        return "SENT";
      case SENT_MESSAGE_TIME:
        return "LAST_SENT";
      case RECEIVED_MESSAGES_5:
        return "RECEIVED5";
      case RECEIVED_MESSAGES_60:
        return "RECEIVED60";
      case RECEIVED_MESSAGES_TOTAL:
        return "RECEIVED";
      case RECEIVED_MESSAGE_TIME:
        return "LAST_RECEIVED";
      case HEARTBEAT_TIME:
        return "HEARTBEAT";
      case MEMORY:
        return "MEM_MB";
      case MEMORY_MAX:
        return "MAX_MEM_MB";
      case RESERVED_CORES:
        return "CORES";
      case RESERVED_MEMORY:
        return "RSV_MEMORY";
      case SESSION_CLIENT_USER:
        return "USER";
      case NODE:
        return "NODE";

      default:
        return "????";
    }
}

std::string dateTime(std::string  timeStr, bool fullDate)
{
    if (timeStr.empty() || (timeStr == "")) {
        return std::string("???");
    }

    /// strip off the fractional amount of time
    auto index = timeStr.find(',');
    if (index != std::string::npos) {
        timeStr = timeStr.substr(0,index);
    }

    // split it into date and time
    std::string dateStr;
    index = timeStr.find(' ');
    if (index != std::string::npos) {
        dateStr = timeStr.substr(0,index);
        timeStr = timeStr.substr(index+1);
    }

    if (fullDate) {
        return dateStr + '-' + timeStr;
    } else {
        // get the current date
        time_t t = time(NULL);
        tm* timePtr = localtime(&t);
        int year = timePtr->tm_year;
        int month = timePtr->tm_mon;
        int day = timePtr->tm_mday;
        char buffer[60];
        sprintf(buffer,"%4d-%02d-%02d", year+1900, month+1, day);
        std::string todayStr(buffer);
        if (dateStr == todayStr) {
            return timeStr;
        } else {
            return dateStr;
        }
    }
}

std::string
PrintComputationStatsCell(const ComputationStats& stats, ColumnType type, bool fullDate, bool comparable)
{
    // add a large offset to integers so that will all be comparable as strings
    // floating point numbers will use alternate printf formats
    // dates will unconditionally use the full date
    unsigned long offset = comparable ? 1000000000000L : 0;

    char buffer[100];
    switch (type) {
      case CPU_USAGE_5:
        if (isnan(stats.mCpuUsage5Secs)) {
            return std::string("???");
        } else {
            sprintf(buffer,comparable ? "%08.1f" : "%.1f", stats.mCpuUsage5Secs/5.0 * 100.0);
            return std::string(buffer);
        }
      case CPU_USAGE_5MAX:
        if (isnan(stats.mCpuUsage5SecsMax)) {
            return std::string("???");
        } else {
            sprintf(buffer,comparable ? "%08.1f" : "%.1f", stats.mCpuUsage5SecsMax/5.0 * 100.0);
            return std::string(buffer);
        }
      case CPU_USAGE_60:
        if (isnan(stats.mCpuUsage60Secs)) {
            return std::string("???");
        } else {
            sprintf(buffer, comparable ? "%08.1f" : "%.1f", stats.mCpuUsage60Secs/60.0 * 100.0);
            return std::string(buffer);
        }
      case CPU_USAGE_60MAX:
        if (isnan(stats.mCpuUsage60SecsMax)) {
            return std::string("???");
        } else {
            sprintf(buffer,comparable ? "%08.1f" : "%.1f", stats.mCpuUsage60SecsMax/60.0 * 100.0);
            return std::string(buffer);
        }
     case CPU_USAGE_TOTAL:
        if (isnan(stats.mCpuUsageTotalSecs)) {
            return std::string("???");
        } else {
            int secs = static_cast<int>(floor(stats.mCpuUsageTotalSecs));
            int hours = secs / 3600;
            secs %= 3600;
            int minutes = secs / 60;
            secs %= 60;
            sprintf(buffer, comparable ? "%04d:%02d:%02d" : "%d:%02d:%02d", hours, minutes, secs);
            return std::string(buffer);
        }
      case SENT_MESSAGES_5:
        if (stats.mSentMessagesCount5Secs < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mSentMessagesCount5Secs + offset);
        }
      case SENT_MESSAGES_60:
        if (stats.mSentMessagesCount60Secs < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mSentMessagesCount60Secs + offset);
        }
      case SENT_MESSAGES_TOTAL:
        if (stats.mSentMessagesCountTotal < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mSentMessagesCountTotal + offset);
        }
      case RECEIVED_MESSAGES_5:
        if (stats.mReceivedMessagesCount5Secs < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mReceivedMessagesCount5Secs + offset);
        }
      case RECEIVED_MESSAGES_60:
        if (stats.mReceivedMessagesCount60Secs < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mReceivedMessagesCount60Secs + offset);
        }
      case RECEIVED_MESSAGES_TOTAL:
        if (stats.mReceivedMessagesCountTotal < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mReceivedMessagesCountTotal + offset);
        }
      case MEMORY:
        if (stats.mMemoryUsageBytes < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mMemoryUsageBytes/1048576 + offset);
        }
      case MEMORY_MAX:
        if (stats.mMemoryUsageBytesMax < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mMemoryUsageBytesMax/1048576 + offset);
        }
      case RESERVED_MEMORY:
        if (stats.mReservedMemory < 0) {
            return std::string("???");
        } else {
            return std::to_string(stats.mReservedMemory/1048576 + offset);
        }
      case RESERVED_CORES:
        if (isnan(stats.mReservedCores)) {
            return std::string("???");
        } else {
            sprintf(buffer,comparable ? "%03.1f" : "%3.1f", stats.mReservedCores);
            return std::string(buffer);
        }
      case SENT_MESSAGE_TIME:
        return dateTime(stats.mLastSentMessageTime, fullDate || comparable);
      case RECEIVED_MESSAGE_TIME:
        return dateTime(stats.mLastReceivedMessageTime, fullDate || comparable);
      case HEARTBEAT_TIME:
        return dateTime(stats.mLastHeartbeatTime, fullDate || comparable);
      case EXEC_STATUS:
        return stats.mExecStatus;
    }
    return std::string();
}

std::string
nodeIdToHostname(const std::string & nodeId, const std::map<std::string,Node>& nodes)
{
    auto iter = nodes.find(nodeId);
    if (iter == nodes.end()) {
        return nodeId;
    } else {
        std::string host = iter->second.mHostname;
        auto index = host.find('.');

        // abbreviate the hostname if it's in the two standard domains
        if ((index != std::string::npos) &&
            ((host.substr(index) == ".anim.dreamworks.com") ||
             (host.substr(index) == ".gld.dreamworks.net"))) {
            return host.substr(0,index);
        } else {
            return host;
        }
    }
}

std::string
PrintComputationCell(
    const Computation& computation,
    const std::map<std::string,Node>& nodes,
    ColumnType type,
    bool fullDate) // don't optimize the date
{

    // see if this is one of the stats columns
    std::string cell = PrintComputationStatsCell(computation.mStats, type, fullDate, false);
    if (!cell.empty()) return cell;

    char buffer[100];
    switch (type) {
      case COMP_NAME:
        return computation.mName;
      case COMP_STATUS:
        return computation.mCompStatus;
      case NODE:
        return nodeIdToHostname(computation.mNodeId, nodes);
      case STOPPED_REASON:
        return computation.mStoppedReason;
      case SIGNAL:
        return computation.mSignal;
      default:
        return std::string();
    }
}

std::string
PrintSessionCell(
    const Session& session,
    const std::map<std::string,Node>& nodes,
    ColumnType type,
    bool fullDate,   // don't optimize the date
    bool comparable) // set to true to return comparable strings rather than pretty ones
{
    // see if this is one of the stats columns
    std::string cell = PrintComputationStatsCell(session.mCompStats, type, fullDate, comparable);
    if (!cell.empty()) return cell;

    switch (type) {
      case FULL_ID:
        if (session.mHasNonDefunct) {
            return session.mId;
        } else {
            return session.mId + "(defunct)";
        }
      case SHORT_ID:
        if (comparable) {
            return session.mId;
        } else {
            if (session.mHasNonDefunct) {
                return session.mId.substr(0,8);
            } else {
                return session.mId.substr(0,8) + "(defunct)";
            }
        }
      case SESSION_CLIENT_USER:
        return session.mClientUser;
      case NODE:
        return nodeIdToHostname(session.mEntryNodeId, nodes);
      default:
        return std::string();
    }
}

//
// a camparator class which can compare two session using the specificied criteria
//
class CompareSessions {
  public:
    CompareSessions(
        const std::vector<ColumnType>& sortOrder,
        const std::vector<bool>& sortDirection,
        const std::map<std::string,Node>& nodes) :
            mSortOrder(sortOrder),
            mSortDirection(sortDirection),
            mNodes(nodes) {}
    bool operator()(const Session& a, const Session& b) {
        for (auto i=0; i < mSortOrder.size(); ++i) {
            std::string aCell = PrintSessionCell(a, mNodes, mSortOrder[i], true, true);
            std::string bCell = PrintSessionCell(b, mNodes, mSortOrder[i], true, true);

            if (aCell < bCell) {
                return !mSortDirection[i];
            } else if (bCell < aCell) {
                return mSortDirection[i];
            }
            // they were equal so continue on to the next sort column
        }

        // they were equal for the purposes of the sort citeria
        return false;
    }

    const std::vector<ColumnType>& mSortOrder;
    const std::vector<bool>& mSortDirection;
    const std::map<std::string,Node>& mNodes;
};
        
    
void
SortSessions(const std::map<std::string,Session>& sessions,
             const std::map<std::string,Node>& nodes,
             std::vector<Session>& sortedSessions,
             const std::vector<ColumnType>& columns,
             const std::vector<bool>& directions)
{
    sortedSessions.clear();
    CompareSessions comparator(columns, directions, nodes);

    // copy the map to  vector for sorting
    for (auto iter=sessions.begin(); iter != sessions.end(); ++iter) {
        const Session& session = iter->second;
        sortedSessions.push_back(session);
    }
    std::sort(sortedSessions.begin(), sortedSessions.end(), comparator);
}

Spreadsheet*
CreateSpreadsheet(const std::vector<Session>& sessions, const std::map<std::string,Node>& nodes,
                 const std::vector<ColumnType>& columns, bool detailed, bool fullDate, bool showLogs)
{
    Spreadsheet* spreadsheet = new Spreadsheet(1, columns.size());
    for (auto i = 0; i < columns.size(); i++) {
        (*spreadsheet)[0][i] = PrintHeading(columns[i]);
    }
    for (auto i = 0; i < columns.size(); i++) {
        (*spreadsheet).setAlignment(i, ChooseAlignment(columns[i]));
    }
    int row = 1;
    for (auto sessIter = sessions.begin(); sessIter != sessions.end(); ++sessIter) {
        const Session& session = *sessIter;
        spreadsheet->addRow();
        if (showLogs) (*spreadsheet)[row].highlight(true);
        for (auto i = 0; i < columns.size(); i++) {
            (*spreadsheet)[row][i] = PrintSessionCell(session, nodes, columns[i], fullDate, false);
        }
        row++;
        if (detailed && session.mHasNonDefunct) {
            for (auto compIter = session.mComputations.begin(); compIter != session.mComputations.end(); ++compIter) {
                const Computation& computation = compIter->second;
                spreadsheet->addRow();
                if (showLogs) (*spreadsheet)[row].highlight(true);
                for (auto i = 0; i < columns.size(); i++) {
                    (*spreadsheet)[row][i] = PrintComputationCell(computation, nodes, columns[i], fullDate);
                }
                row++;
            }
        }
        if (showLogs) {
            for (auto iter = session.mLogLines.begin(); iter != session.mLogLines.end(); ++iter) {
                spreadsheet->addRow();
                (*spreadsheet)[row] = std::string("    ") + *iter;
                row++;
            }
        }
    }

    return spreadsheet;
}

