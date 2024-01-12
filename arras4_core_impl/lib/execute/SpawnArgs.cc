// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "SpawnArgs.h"

#include <unistd.h>

namespace arras4 {
    namespace impl {

void SpawnArgs::setCurrentWorkingDirectory()
{
    char* cwd = getcwd(nullptr,0);
    if (cwd) {
	workingDirectory = cwd;
	free(cwd);
    }
}

std::string SpawnArgs::debugString(unsigned maxEnvirVars, bool options) const
{
    std::string ret(program);
    for (std::string arg : args) {
        ret += " '" + arg + "'";
    }
    if (maxEnvirVars > 0) {
        ret += "\n[";
	unsigned i = 0;
	for (const auto& entry : environment.map()) {
            ret += " " + entry.first  + "=" + entry.second + ";";
	    if (i++ >= maxEnvirVars) {
		ret += "...";
		break;
	    } 
        }
        ret += "]";
    }
    if (options) {
        ret += "\n(";
        if (!workingDirectory.empty()) 
            ret += " cwd='"+workingDirectory+"';";
        if (cleanupProcessGroup)
            ret += " cleanupPG;";
        if (ioCapture)
            ret += " ioCapture;";
        ret += " Mb="+std::to_string(assignedMb);
        if (enforceMemory)
            ret += "(enforced)";
        ret += " Cores="+std::to_string(assignedCores);
        if (enforceCores)
            ret += "(enforced)";
        ret += ")";
    }
    return ret;
}

// try to find the given executable name on the PATH
// in our environment. If found, use it to set 'program' and
// return true. OW leave 'program' unchanged and return false
bool SpawnArgs::findProgramInPath(const std::string& name)
{
    const std::string plist = environment.get("PATH");
    if (plist.empty())
	return false;
    size_t p = 0;
    while (true) {
	size_t n = plist.find(':',p);
	if (n > p) {
	    std::string path = plist.substr(p,n-p);
	    path += "/" + name;
	    if (access(path.c_str(),X_OK)==0) {
		program = path;
		return true;
	    }
	}
	if (n >= plist.size()) break;
	p = n+1;
    }
    return false;
}

}
}
