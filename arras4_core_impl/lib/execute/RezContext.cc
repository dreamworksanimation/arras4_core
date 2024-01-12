// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0


#include "RezContext.h"
#include "ProcessManager.h"
#include "IoCapture.h"
#include "SpawnArgs.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <unistd.h> // unlink
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <cctype> // iscntrl

namespace {
    const std::string REQ_VERSION_1("1.7.0");
    const std::string REQ_PACKAGES_PATH_1("/rel/packages:/rel/lang/python/packages");

    // this is the default pseudo-compiler for rez1 :
    // if "pseudo-compiler" is set to something else, 
    // we must add it as a suffix to the binary name
    const std::string DEFAULT_PSEUDO_COMPILER("icc150_64");


    const std::string ENV_REQ_VERSION_2("REZ2_DEFAULT_VERSION");
    const std::string REQ_PACKAGES_PATH_2("/rel/rez/dwa:/rel/rez/third_party:/rel/lang/python/packages");

    // standard paths required for REZ scripts to run
    const std::string BASE_PATH = "/bin:/usr/bin:/usr/local/bin";

    const std::chrono::milliseconds REZ_CONFIG_TIMEOUT(240000); // 4 min
    // Rez resolution can take a long time in some cases. In the end, Coordinator will give
    // up on waiting for the computation to start, but this doesn't necessarily terminate
    // the rez config process so we add a timeout specifically for this.

}

namespace arras4 {
    namespace impl {

RezContext::RezContext(const std::string& name,
                       unsigned majorVersion,
                       const std::string& packagesPathPrefix,
		       bool omitDefaultPackagePath,
		       const std::string& pseudoCompiler,
                       const api::UUID& id, 
                       const api::UUID& sessionId) : 
    mName(name), mId(id), mSessionId(sessionId), mMajorVersion(majorVersion),
    mPackagesPath(packagesPathPrefix),mPseudoCompiler(pseudoCompiler)
{
    mIoCapture = std::make_shared<SimpleIoCapture>();

    if (mId.isNull())
        mId.generate();
    if (mSessionId.isNull())
        mSessionId = mId;

    if (!omitDefaultPackagePath && !mPackagesPath.empty())
        mPackagesPath += ":";

    if (majorVersion == 1) {
        mVersion = REQ_VERSION_1;
	if (!omitDefaultPackagePath)
	    mPackagesPath += REQ_PACKAGES_PATH_1;
        mRezDir = "/rel/third_party/rez/" + mVersion;
        mBinDir = mRezDir + "/bin/";
    } 
    else if (majorVersion == 2) {
        const char* req_version = std::getenv(ENV_REQ_VERSION_2.c_str());
        if (req_version) {
            mVersion = req_version; 
	    if (!omitDefaultPackagePath)
		mPackagesPath += REQ_PACKAGES_PATH_2;
            mRezDir = "/rel/third_party/rez/" + mVersion;
            mBinDir = mRezDir + "/bin/rez/";
        } else {
            throw std::runtime_error("Environment variable " + ENV_REQ_VERSION_2 + " is not set");
        }
    } else {
        throw std::runtime_error("Unsupported major rez version: " + std::to_string(majorVersion));
    }
}

RezContext::~RezContext()
{
}

const std::string& RezContext::getContextFile()
{
    return mContextFilePath;
}

// basic function to perform package resolution using
// an external process
bool RezContext::doPackageResolve(ProcessManager& procMan,
				  const std::string& packages,
				  std::string& error)
{
    // set up a process to resolve the package list to a context
    SpawnArgs args;
    std::istringstream iss(packages);
    for(std::string package; iss >> package; )
        args.args.push_back(package);

    if (mMajorVersion == 1) {
        args.program = mBinDir + "rez-config";
        args.args.push_back("--print-env");
    } else {
        args.program = mBinDir + "rez-env";
        args.args.push_back("--output");
        args.args.push_back("-");
    }

    // get the correct environment to run rez
    getRezEnvironment(args.environment);

    // capture rez output
    mIoCapture->clear();
    args.ioCapture = mIoCapture;
 
    ARRAS_DEBUG(log::Session(mSessionId.toString()) <<
               "Resolving rez context: " << args.debugString());
 
    // run the process
    api::UUID id = api::UUID::generate();
    std::string name = mName+"-rez_config-" + id.toString();
    Process::Ptr pp = procMan.addProcess(id,name,id);
    StateChange sc = pp->spawn(args);
    if (!StateChange_success(sc)) {
        ARRAS_ERROR(log::Id("rezConfigFail") <<
                    log::Session(mSessionId.toString()) <<
                    "Failed to start rez-config for " << mName);
        procMan.removeProcess(id);
        return false;
    }
    ExitStatus es;
    bool finished = pp->waitForExit(&es,REZ_CONFIG_TIMEOUT);
    procMan.removeProcess(id);

    // error conditions
    std::string allErrors;
    if (!finished) {
        allErrors += " : process timed out";
    }
    else {
        if (es.status != 0) { 
            allErrors += " : error code " + std::to_string(es.status);
        }
        if (mIoCapture->out().empty()) {
            allErrors += " : no output context was  produced";
        }
    }
    if (allErrors.empty()) {
        return true;
    } else {
        error = allErrors;
	if (!mIoCapture->err().empty()) {
            error += " : error output [" + mIoCapture->err() + "]";
        }
    }
    return false;
}

// more direct access to the doPackageResolve function
// returns empty string on failure
std::string RezContext::resolvePackages(ProcessManager& procMan,
					const std::string& packages,
					std::string& error)
{
    bool ok = doPackageResolve(procMan,packages,error);
    if (ok)
	return mIoCapture->out();
    return std::string();
}

// call this to set the context from a list of package names.
// Overwrites the effect of setContext or setContextFile
bool RezContext::setPackages(ProcessManager& procMan,
                             const std::string& packages,
                             std::string& error)
{
    bool ok = doPackageResolve(procMan,packages,error);
    if (ok)
	return setContext(mIoCapture->out(),error);
    return false;
}

// call this to set the context to a string. 
// Overwrites the effect of setPackages or setContextFile
bool RezContext::setContext(const std::string& context,
                            std::string& error)
{    
    mContextFilePath = "/tmp/generated_rezCtxt_" + mName + "-" + mId.toString();
    try {
        std::ofstream out;
        out.open(mContextFilePath.c_str());
        if (!out.is_open()) { 
            error = "failed to open file " + mContextFilePath + " for writing";
            return false;
        }
        out << context;
        out.close();
        if (out.bad()) { 
            error = "failure writing context file " + mContextFilePath;
            return false;
        }
        return true;
    } catch (...) { 
        error = "failure writing context file " + mContextFilePath;
        return false;
    }
    
}

// call this to set the context to a file. 
// Overwrites the effect of setPackages or setContext
bool RezContext::setContextFile(const std::string& filepath,
                                std::string& /*error*/)
{
    mContextFilePath = filepath;
    return true;
}
   
bool RezContext::wrap(const SpawnArgs& in, SpawnArgs& out)
{
    if (&out != &in) {
        out = in;
    }

    // build -c command argument
    std::string cmdstr(in.program);
    
    // rez1 requires pseudo-compiler suffix
    if (mMajorVersion == 1 &&
	!mPseudoCompiler.empty() &&
	!(mPseudoCompiler == DEFAULT_PSEUDO_COMPILER)) {
	cmdstr += "-" + mPseudoCompiler;
    }
    
    for (auto& arg : in.args) {
        cmdstr += " ";
        cmdstr += arg;
    }
    
    // set program and args for out
    out.program = mBinDir + "rez-env";
    out.args.clear();
   
    if (mMajorVersion == 1) {
	// ARRAS-3556 : run baked context under bash, not rez-env
	// this avoids unnecessary file access attempts when running
	// on a cloud machine
	std::string bashcmd("source ");
	bashcmd += mContextFilePath;
	bashcmd += " && ";
	bashcmd += cmdstr;
	out.program = "/bin/bash";
	out.args.clear();
	
        out.args.push_back("-c");
        out.args.push_back(bashcmd);
	getRezEnvironment(out.environment);
	getBashEnvironment(out.environment);
    } else {
        out.args.push_back("--input");
        out.args.push_back(mContextFilePath);
	out.args.push_back("-c");
	out.args.push_back(cmdstr);
	getRezEnvironment(out.environment);
    }
    return true;
}

// append the environment variables needed to run rez
void RezContext::getRezEnvironment(Environment& e) const
{
    e.set("REZ_VERSION",mVersion);  
    e.set("REZ_LOCK_PACKAGES_PATH","1");
    e.set("REZ_KEEP_TMPDIRS","1");  
    e.set("REZ_PACKAGES_PATH",mPackagesPath);
    e.set("REZ_LOCAL_PACKAGES_PATH","");
    e.set("REZ_PLATFORM","Linux");
    e.set("REZ_PATH",mRezDir);
    e.set("PATH",mBinDir + ":./:" + BASE_PATH);
        
    if (mMajorVersion == 2) {
	// ARRAS-3712 : config now requires OS_RELEASE to be set
	// Note that this means you can't use resolutions across
	// different os releases
	const char* os_rel = std::getenv("OS_RELEASE");
	if (os_rel) {
	    e.set("OS_RELEASE",os_rel);
	} 
        e.set("REZ_CONFIG_FILE","/rel/boot/rez/config/" + 
	      mVersion + "/config_pipex.py");
    }
}

// append the environment variables needed to run under bash
// for now we're assuming the program expects to run under REZ, 
// even though it isn't, so we include the "rez environment"
void RezContext::getBashEnvironment(Environment& e) const
{

    // rez1 requires "PSEUDO_NAME" to be defined
    if (mMajorVersion == 1 &&
	!mPseudoCompiler.empty() &&
	!(mPseudoCompiler == DEFAULT_PSEUDO_COMPILER)) {
	e.set("PSEUDO_NAME",mPseudoCompiler);
    }
}

}
}
