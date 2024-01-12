// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "process_utils.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <cstring>
#include <dirent.h>
#include <fstream>

namespace arras4 {
    namespace impl {

std::string
get_file_contents(const std::string& filename)
{
    std::ifstream in(filename.c_str(), std::ios::in);
    if (in) {
        const int STATSIZE = 512;
        // stat() doesn't work properly on the stat file since it is virtual file
        // so just read a big enough size to be sure to fit an entire standard stat file
        char buffer[STATSIZE];
        memset(buffer, 0, STATSIZE);

        // read all of the available data
        in.read(buffer, STATSIZE);
        in.close();

        // return it as a string
        std::string result(buffer);
        return result;
    }

    return std::string();
}

int countProcessGroupMembers(pid_t aPgid)
{
    struct dirent* result = NULL;
    struct dirent entry;

    // not being able to open the direction should be impossible unless there is something crazy
    // like no more file handles but handle it for robustness and just use a count of zero.
    DIR* dir = opendir("/proc");
    if (dir == NULL) {
        ARRAS_ERROR(log::Id("OpenProcFail") <<
                    "Unable to open /proc to count session survivors. Assuming none.");
        return 0;
    }

    int memberCount = 0;
    do {
        readdir_r(dir, &entry, &result);
        if (result != NULL) {
            if (isdigit(entry.d_name[0])) {
                std::string name = std::string("/proc/") + entry.d_name + "/stat";
                std::string statusLine = get_file_contents(name);

                // the process could go away while the list is being processed
                if (statusLine.empty()) continue;

                // find the right paren
                std::string::size_type index = statusLine.rfind(')');;
                if (index == std::string::npos) continue;

                // skip over " %c "
                index = statusLine.find(' ', index + 4);

                // skip over a number
                if (index == std::string::npos) continue;
                index++;

                // get the PGID
                std::string::size_type index2 =  statusLine.find(' ', index);
                if (index2 == std::string::npos) continue;
                pid_t pgid = static_cast<pid_t>(atol(statusLine.substr(index, index2-index).c_str()));

                if (aPgid == pgid) memberCount++;
            }
        }
    } while (result !=NULL);
    closedir(dir);

    return memberCount;
}

bool doesProcessGroupHaveMembers(pid_t aPgid)
{
    return countProcessGroupMembers(aPgid) > 0;
}

}
}
