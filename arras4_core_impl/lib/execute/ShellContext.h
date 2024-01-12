// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef SHELL_CONTEXT_H_
#define SHELL_CONTEXT_H_

#include "Process.h"

#include <string>
#include <memory>
#include <vector>

namespace arras4 {
    namespace impl {

// Allows you to run a program in a shell context. Potentially supports multiple different shells, but currently
// only bash. The "context" is defined by a script sourced before running the program. You can set this up by calling setScript,
// which writes the string you pass to a temporary file.
//
// also supports a pseudo compiler suffix (e.g. "iccHoudini165_64") which is appended to the command name. This
// is required for rez1 configurations to function correctly.
//
// After setup, call the wrap() function, passing in SpawnArgs describing the program you want to run. ShellContext
// will generate a new set of SpawnArgs that will, when used, run the program under a shell, first sourcing the script.
// You can use the same SpawnArgs as input and output, in which case it will be modified in place.
// 

enum class ShellType 
{
    Bash 
};

class ShellContext
{
public:
    
    ShellContext(ShellType type,
		 const std::string& pseudoCompiler,
		 const api::UUID& sessionId = api::UUID()); // for logging;

    ~ShellContext();

    bool setScript(const std::string& script,
		      std::string& error);
  
    // get the script file path.
    const std::string& getScriptFile();

    // call this to wrap a command so that it runs in the context
    // in and out may be the same object
    bool wrap(const SpawnArgs& in, SpawnArgs& out);

private:
    
    ShellType mShellType;
    std::string mPseudoCompiler;
    api::UUID mSessionId;       // used when logging
    std::string mScriptFilePath; // path of a file holding script
};

}
}
#endif
