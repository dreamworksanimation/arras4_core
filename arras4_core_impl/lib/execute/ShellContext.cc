// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0


#include "ShellContext.h"
#include "SpawnArgs.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <fstream>

namespace arras4 {
    namespace impl {
  
ShellContext::ShellContext(ShellType type,
			   const std::string& pseudoCompiler,
			   const api::UUID& sessionId)
    : mShellType(type),
      mPseudoCompiler(pseudoCompiler),
      mSessionId(sessionId)
{
    if (mSessionId.isNull())
	mSessionId.generate();
}

ShellContext::~ShellContext()
{
}

const std::string& ShellContext::getScriptFile()
{
    return mScriptFilePath;
}

// call this to set the script
bool ShellContext::setScript(const std::string& script,
			     std::string& error)
{    
    api::UUID id;
    id.generate();
    mScriptFilePath = "/tmp/generated_script_" + id.toString();
    try {
        std::ofstream out;
        out.open(mScriptFilePath.c_str());
        if (!out.is_open()) { 
            error = "failed to open file " + mScriptFilePath + " for writing";
            return false;
        }
        out << script;
        out.close();
        if (out.bad()) { 
            error = "failure writing script file " + mScriptFilePath;
            return false;
        }
        return true;
    } catch (...) { 
        error = "failure writing script file " + mScriptFilePath;
        return false;
    }
    
}
   
bool ShellContext::wrap(const SpawnArgs& in, SpawnArgs& out)
{
    if (&out != &in) {
        out = in;
    }

    // build -c command argument
    std::string cmdstr(in.program);
    if (!mPseudoCompiler.empty())
	cmdstr += "-"+mPseudoCompiler;

    for (auto& arg : in.args) {
        cmdstr += " ";
        cmdstr += arg;
    }
    
    // set program and args for out
    out.program = "/bin/bash";

    std::string bashcmd("source ");
    bashcmd += mScriptFilePath;
    bashcmd += " && ";
    bashcmd += cmdstr;

    out.args.clear();
    out.args.push_back("-c");
    out.args.push_back(bashcmd);

    return true;
}

}
}
