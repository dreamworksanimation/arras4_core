// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "LocalSession.h"
#include "SessionError.h"

#include <message_api/Object.h>
#include <network/IPCSocketPeer.h>
#include <arras4_log/Logger.h>
#include <arras4_athena/AthenaLogger.h>
#include <arras4_log/LogEventStream.h>

#include <execute/ProcessManager.h>
#include <execute/RezContext.h>

#include <shared_impl/RegistrationData.h>
#include <shared_impl/ProcessExitCodes.h>

#include <unistd.h>
#include <thread>
#include <string>
#include <fstream>
#include <signal.h>

#if defined(JSONCPP_VERSION_MAJOR)
#define memberName name
#endif

namespace {
    const unsigned DEFAULT_MEMORY_MB = 2048;
    const unsigned RESERVED_CORES = 1;
    const int DEFAULT_LOG_LEVEL = 3;
    const char* ATHENA_ENV = "prod";
    const char* ATHENA_HOST = "localhost";
    const char* ATHENA_PORT = "514";

    unsigned CONNECT_TIMEOUT_MS = 20000;
    unsigned NEGOTIATION_TIMEOUT_MS = 5000;

    unsigned short ARRAS_MESSAGING_API_VERSION_MAJOR=4;

    const char* ENV_OVR_LOCAL_PACKAGE_PATH = "ARRASCLIENT_OVR_LOCAL_PACKAGE_PATH_PREFIX";

    std::string getString(arras4::api::ObjectConstRef obj,
		       const std::string& key,
		       const std::string& def = std::string())
    {
	if (obj.isObject() &&
	    obj[key].isString())
	    return obj[key].asString();
	return def;
    }

    std::string exitStatusString(arras4::impl::ExitStatus es, bool expected)
    {
	if (es.exitType == arras4::impl::ExitType::Exit) {
            return arras4::impl::exitCodeString(es.status,expected);
	} else if (es.exitType == arras4::impl::ExitType::Signal) {
	    // sig 15 (term) indicates successful termination when it is expected
	    if (expected && es.status == 15)
		return "exited normally";
	    // strsignal doesnt seem threadsafe
	    std::string ret = "exited due to signal " + std::to_string(es.status);
	    return ret;
	}
	return arras4::impl::ExitStatus::internalCodeString(es.status);
    }
}

namespace arras4 {
    namespace client {

LocalSession::LocalSession(impl::ProcessManager& procman, const std::string& aSessionId) :
    mProcessManager(procman)
{
    mAddress.session = aSessionId;
    mAddress.node = api::UUID::generate();
    mAddress.computation = api::UUID::generate();
}

LocalSession::~LocalSession()
{
    // can't delete while connecting
    {
	std::unique_lock<std::mutex> lock(mConnectMutex);
	while (mConnecting) {
	    mConnectCondition.wait(lock);
	}
    }
    if (mConnectThread.joinable())
	mConnectThread.join();
}

// get an object by key from JSON config data. Returns an empty object if the
// key doesn't exist or value is not an object
api::ObjectConstRef LocalSession::getObject(api::ObjectConstRef obj,
                                                 const std::string& key)
{
    api::ObjectConstRef ret = obj[key];
    if (ret.isObject())
        return ret;
    ARRAS_WARN(log::Id("warnBadConfigVal") <<
               log::Session(mAddress.session.toString()) <<
               "In config for " << mName << ": item " << 
               key << " should be an object");
    return api::emptyObject;
}

// throws SessionError
void LocalSession::setDefinition(api::ObjectConstRef def)
{
    api::ObjectConstRef comps = def["computations"];
    if (!comps.isObject() ||
     	!(comps.size() == 2) ||
     	(comps["(client)"].isNull())) {	
    	throw SessionError("Local session definitions must contain client and one computation");
    }

    api::ObjectConstRef contexts = def["contexts"];
    for (api::ObjectConstIterator cIt = comps.begin();
         cIt != comps.end(); ++cIt) {
	std::string name = cIt.memberName();
	if (name != "(client)") {
	    processComputation(name,*cIt,contexts);
	}	    
    }
    
    buildRouting(comps["(client)"]);
}

void LocalSession::buildRouting(api::ObjectConstRef clientDef)
{
    if (!clientDef.isObject())
	throw SessionError("Invalid client definition");
   
    api::ObjectRef mapEntry = mExecConfig["routing"][mAddress.session.toString()]["computations"][mName];
    mapEntry["compId"] = mAddress.computation.toString();
    mapEntry["nodeId"] = mAddress.node.toString();
   
    api::ObjectRef cfilter = mExecConfig["routing"]["messageFilter"][mName];
    // cppcheck-suppress redundantAssignment 
    cfilter = api::emptyObject;
    api::ObjectConstRef messages = clientDef["messages"]; 
    if (messages.isObject()) {
	api::ObjectConstRef sfilter = messages[mName];
	if (sfilter.isObject())
	    cfilter["(client)"] = sfilter;
	// note: if sfilter is "*" then cfilter will be an empty object,
	// which allows through all messages.
    }
}

void LocalSession::processComputation(const std::string& name,
				      api::ObjectConstRef definition,
                                      api::ObjectConstRef contexts)
{
    mName = name;
    mExecConfigFilePath = "/tmp/exec-" + name + "-" + mAddress.computation.toString();
    mIpcAddress = mExecConfigFilePath + ".ipc";

    mSpawnArgs = impl::SpawnArgs();

    api::ObjectConstRef requirements = getObject(definition,"requirements");
    api::ObjectConstRef resources = getObject(requirements,"resources");
   

    mSpawnArgs.program = "execComp";

    mSpawnArgs.enforceMemory = false;
    mSpawnArgs.enforceCores = false;

    api::ObjectConstRef memVal = resources["memoryMB"];
    if (memVal.isNumeric() && (memVal.asInt64() >= 0)) 
	mSpawnArgs.assignedMb = memVal.asUInt();
    else 
	mSpawnArgs.assignedMb = DEFAULT_MEMORY_MB;

    api::ObjectConstRef coresVal = resources["cores"];
    if (coresVal.isNumeric() && (coresVal.asInt64() >= 0)) 
        mSpawnArgs.assignedCores = coresVal.asUInt();
    else {
        unsigned maxCores = 1024;
        api::ObjectConstRef maxVal = resources["maxCores"];
        if (maxVal.isNumeric() && (maxVal.asInt64() >= 0)) 
            maxCores = maxVal.asUInt();
        unsigned reservedCores = RESERVED_CORES;
        api::ObjectConstRef reservedVal = resources["reservedCores"];
        if (reservedVal.isNumeric() && (reservedVal.asInt64() >= 0)) 
            reservedCores = reservedVal.asUInt();
	mSpawnArgs.assignedCores = std::thread::hardware_concurrency() - reservedCores;
	if (mSpawnArgs.assignedCores > maxCores)
	    mSpawnArgs.assignedCores = maxCores;
    }

    api::ObjectConstRef wdVal = definition["workingDirectory"];
    if (wdVal.isString())	
	mSpawnArgs.workingDirectory = wdVal.asString();
 
    mSpawnArgs.cleanupProcessGroup = true;

    // localComp arguments...
    auto& args = mSpawnArgs.args;

    // memory and core limits are passed to execComp as arguments, as well
    // as being part of SpawnArgs
    args.push_back("--memoryMB");
    args.push_back(std::to_string(mSpawnArgs.assignedMb));
    args.push_back("--cores");
    args.push_back(std::to_string(mSpawnArgs.assignedCores));
    args.push_back("--use_affinity");
    args.push_back("0");
   
    
    // the config file contains additional configuration information
    args.push_back(mExecConfigFilePath);

    // set up environment
    auto& env = mSpawnArgs.environment;
    // highest priority is environment specified for this computation
    api::ObjectConstRef environment = getObject(definition,"environment");
    env.setFrom(environment);
    // next is environment specified in context
    std::string ctxName;
    if (requirements["context"].isString()) {
	ctxName = requirements["context"].asString();
	if (!contexts.isMember(ctxName)) 
	    throw SessionError("Context '" + ctxName + "', required by " + 
			       name + " is missing");
	
	api::ObjectConstRef ctxEnv = getObject(contexts[ctxName],"environment");
	env.setFrom(ctxEnv);
    }
    // finally some specific required values
    env.set("ARRAS_ATHENA_ENV",ATHENA_ENV);
    env.set("ARRAS_ATHENA_HOST",ATHENA_HOST);
    env.set("ARRAS_ATHENA_PORT",ATHENA_PORT); 
    char* user = getenv("LOGNAME");
    if (user != NULL) {
        env.set("USER",user);
    }
    char* client_root= getenv("REZ_ARRAS4_CORE_ROOT");
    if (client_root != NULL) {
        env.set("ARRAS_BREAKPAD_PATH",client_root);
    }

    if (ctxName.empty())
	applyPackaging(definition);
    else
	applyPackaging(definition,contexts[ctxName]);

    int logLevel = DEFAULT_LOG_LEVEL;
    api::ObjectConstRef logVal = resources["logLevel"];
    if (logVal.isNumeric())
	logLevel = logVal.asInt();

    // mExecConfig holds the contents of the config file, which we
    // will write out to mExecConfigFilePath
    mExecConfig = api::Object();
    mExecConfig["sessionId"] = mAddress.session.toString();
    mExecConfig["compId"] = mAddress.computation.toString();
    mExecConfig["execId"] = mAddress.computation.toString();
    mExecConfig["nodeId"] = mAddress.node.toString();
    mExecConfig["ipc"] = mIpcAddress;
    mExecConfig["logLevel"] = logLevel;
    // logs echoed to the console in local mode are in a shorter, more user-friendly form
    mExecConfig["consoleLogStyle"] =  static_cast<unsigned>(arras4::log::ConsoleLogStyle::Short);
    mExecConfig["config"][mName] = definition;
    mExecConfig["config"][mName]["computationId"] = mAddress.computation.toString();
}

void LocalSession::applyPackaging(api::ObjectConstRef definition,
				  api::ObjectConstRef context)
{
    api::ObjectConstRef requirements = getObject(definition,"requirements");
    api::ObjectConstRef ctx = (context.isNull()) ? requirements : context;

    std::string packagingSystem = getString(ctx,"packaging_system");

    // "requirements" defaults to packaging system "rez1",
    // context defaults to none
    if (context.isNull() && packagingSystem.empty())
	packagingSystem = "rez1";
                                    
    if (packagingSystem.empty() || packagingSystem == "none") {
	applyNoPackaging(ctx);
    }
    else if (packagingSystem == "current-environment") {
	applyCurrentEnvironment(ctx);
    }
    else if (packagingSystem == "bash") {
	applyShellPackaging(impl::ShellType::Bash,ctx);
    }
    else if (packagingSystem == "rez1") {
	applyRezPackaging(1,ctx);
    } 
    else if (packagingSystem == "rez2") {
        applyRezPackaging(2,ctx);
    } 
    else {
        ARRAS_WARN(log::Id("warnUnknownPackaging") <<
                   log::Session(mAddress.session.toString()) <<
                   "In config for " << mName << ": unknown packaging system '" << 
                   packagingSystem << "'");
        throw SessionError("Unknown packaging system '" + packagingSystem + "'");
    }
}

void LocalSession::applyNoPackaging(api::ObjectConstRef ctx)
{
    // if no packaging is specified, we will run execComp directly
    // without a shell wrapper. To do this, we need to locate the executable
    // within the PATH in the computation environment
    std::string program = mSpawnArgs.program;
    std::string pseudoCompiler = getString(ctx,"pseudo-compiler");
    if (!pseudoCompiler.empty()) {
	program += "-" + pseudoCompiler;
    }
    bool ok = mSpawnArgs.findProgramInPath(program);
    if (!ok) {
	ARRAS_ERROR(log::Id("ExecFail") <<
		    log::Session(mAddress.session.toString()) <<
		    " : cannot find executable " << program << 
		    " on PATH for " << mName); 
	throw SessionError("Execution error");
    }
}

void LocalSession::applyCurrentEnvironment(api::ObjectConstRef ctx)
{
    mSpawnArgs.environment.setFromCurrent();
    std::string pseudoCompiler = getString(ctx,"pseudo-compiler");
    if (!pseudoCompiler.empty()) {
	mSpawnArgs.program += "-" + pseudoCompiler;
    }
}

void LocalSession::applyShellPackaging(impl::ShellType type,api::ObjectConstRef ctx)
{
    std::string shellScript = getString(ctx,"script");
    if (shellScript.empty()) {
	ARRAS_ERROR(log::Id("ShellWrapFail") <<
		    log::Session(mAddress.session.toString()) <<
		    " : Must specify shell script for " << mName); 
	throw SessionError("Shell wrap error");
    }
    std::string pseudoCompiler = getString(ctx,"pseudo-compiler");
    try {
	impl::ShellContext sc(type,
			      pseudoCompiler,
			      mAddress.session);
        std::string err;
        bool ok = sc.setScript(shellScript,err);
        if (ok) {
            ok = sc.wrap(mSpawnArgs,mSpawnArgs);
            if (!ok) {
                ARRAS_ERROR(log::Id("ShellWrapFail") <<
                            log::Session(mAddress.session.toString()) <<
			    " : Failed to wrap " << mName); 
                throw SessionError("Shell wrap error");
            } 
        } else {
            ARRAS_ERROR(log::Id("ShellSetupFail") <<
                        log::Session(mAddress.session.toString()) <<
                        " : Failed to setup shell environment for " << mName 
                        << " : " << err); 
            throw SessionError("Shell wrap error" + err);
        }
    } catch (std::exception& e) {
        ARRAS_ERROR(log::Id("ShellSetupFail") <<
                    log::Session(mAddress.session.toString()) <<
                    " : Failed to setup shell environment for " << mName 
                    << " : " << e.what()); 
        throw SessionError(e.what());
    }
}

void LocalSession::applyRezPackaging(unsigned rezMajor,
				     api::ObjectConstRef ctx)
{
    std::string pseudoCompiler = getString(ctx,"pseudo-compiler");

    std::string rezPathPrefix;
    const char* ppoVal = getenv(ENV_OVR_LOCAL_PACKAGE_PATH);
    if (ppoVal) {
	rezPathPrefix = ppoVal;
    } else {
	rezPathPrefix = getString(ctx,"rez_packages_prepend");
    }
    std::string rezPackages = getString(ctx,"rez_packages");
    std::string rezContext = getString(ctx,"rez_context");
    std::string rezContextFile = getString(ctx,"rez_context_file");

    try {
	impl::RezContext rc(mName, rezMajor, rezPathPrefix, 
			    false,
			    pseudoCompiler,
			    mAddress.computation, 
			    mAddress.session);
 
        bool ok = false;
        std::string err;
        if (!rezContext.empty()) ok = rc.setContext(rezContext,err);
        else if (!rezContextFile.empty()) ok = rc.setContextFile(rezContextFile,err);
        else if (!rezPackages.empty()) ok = rc.setPackages(mProcessManager,rezPackages,err);
        else err = "Must specify one of 'rez_context','rez_context_file' or 'rez_packages'";
        if (ok) {
            ok = rc.wrap(mSpawnArgs,mSpawnArgs);
            if (!ok) {
                ARRAS_ERROR(log::Id("RezWrapFail") <<
                            log::Session(mAddress.session.toString()) <<
                            "[ rez" << rezMajor << "] Failed to rez wrap " << mName); 
                throw SessionError("Packaging failure");
            } 
        } else {
            ARRAS_ERROR(log::Id("RezSetupFail") <<
                        log::Session(mAddress.session.toString()) <<
                        "[ rez" << rezMajor << "] Failed to setup rez environment for " << mName 
                        << " : " << err); 
            throw SessionError("Rez error" + err);
        }
    } catch (std::exception& e) {
        ARRAS_ERROR(log::Id("RezSetupFail") <<
                    log::Session(mAddress.session.toString()) <<
                    "[ rez" << rezMajor << "] Failed to setup rez environment for " << mName 
                    << " : " << e.what()); 
        throw SessionError(e.what());
    }
}

bool LocalSession::writeConfigFile()
{
    ARRAS_DEBUG("Saving config to " << mExecConfigFilePath);
    std::ofstream ofs(mExecConfigFilePath);
    if (!ofs.fail()) {   
        try { 
            std::string s = api::objectToString(mExecConfig);
            ofs << s;
            return true;
        } catch (...) {
            // fall through to error handling
        }
    }
    ARRAS_ERROR(log::Id("configFileSaveFail") <<
                log::Session(mAddress.session.toString()) <<
                "Failed to save config file: " <<
                mExecConfigFilePath);
    return false;
}
   
// throws SessionError
void LocalSession::spawnProcess()
{
    ARRAS_ATHENA_TRACE(0,log::Session(mAddress.session.toString()) <<
                       "{trace:comp} launch " << mAddress.computation.toString() << 
                       " " << mName);

    bool ok = writeConfigFile();
    if (!ok) {
        // config has already logged the reason
        throw SessionError("Cannot start computation " + mName+ 
			   " [" + mAddress.computation.toString()+"] : failed to save config file");
    }
  
    mProcess = mProcessManager.addProcess(mAddress.computation,
					  mName,
					  mAddress.session);
    if (!mProcess) {
        ARRAS_ERROR(log::Id("processObjectCreateFail") <<
                    log::Session(mAddress.session.toString()) <<
                    "Failed to create Process object for " << mName);
        throw SessionError("Failed to create Process object");
    }

    impl::StateChange sc = mProcess->spawn(mSpawnArgs);
    if (!impl::StateChange_success(sc)) {
        ARRAS_ERROR(log::Id("processSpawnFail") <<
                    log::Session(mAddress.session.toString()) <<
                    "Failed to spawn process for " << mName);
        throw SessionError("Cannot start computation " + mName+ 
			   " [" + mAddress.computation.toString()+"] : process spawn failed");
    } 
}   


void LocalSession::connectProc()
{
    unlink(mIpcAddress.c_str());
    network::IPCSocketPeer listenPeer;
    listenPeer.listen(mIpcAddress);
    network::Peer* peer = nullptr;
    network::Peer** peers = &peer;
    int count = 1;        
    listenPeer.accept(peers,count,CONNECT_TIMEOUT_MS);
    {
	std::unique_lock<std::mutex> lock(mConnectMutex);
	mPeer.reset(peer);
	mConnecting = false;
    }
    mConnectCondition.notify_all();
}
	
void LocalSession::start(std::shared_ptr<LocalSession> shared_this,
			 TerminateFunc tf)
{
    {
	std::unique_lock<std::mutex> lock(mCallbackMutex);
	mTerminateCallback = tf;
    }
    mSpawnArgs.observer = shared_this;
    
    // start listening for computation to connect
    mConnecting = true;
    mConnectThread = std::thread(&LocalSession::connectProc,this);

    // spawn computation
    spawnProcess();
    
    // wait for computation to connect back
    std::unique_lock<std::mutex> lock(mConnectMutex);
    while (mConnecting) {
	mConnectCondition.wait(lock);
    }
  
    if (mConnectThread.joinable())
	mConnectThread.join();

    if (!mPeer) {
	throw SessionError("Computation failed to connect within timeout");
    }

    readRegistration();
}

void LocalSession::abandon()
{
    // used during shutdown to prevent termination 
    // callback after client is deleted, without
    // the need to wait for session to stop
    {
	std::unique_lock<std::mutex> lock(mCallbackMutex);
	mTerminateCallback = TerminateFunc();
    }
}

void LocalSession::readRegistration()
{
    impl::RegistrationData regData(0,0,0);
    try {
	mPeer->receive_all_or_throw((char*)(&regData),sizeof(regData),
				    "LocalSession::readRegistration",
				    NEGOTIATION_TIMEOUT_MS);

    } catch (std::exception& e) {
	throw SessionError("Failed to register computation: "+std::string(e.what()));
    }
    if (regData.mMagic != impl::RegistrationData::MAGIC ||
	regData.mMessagingAPIVersionMajor != ARRAS_MESSAGING_API_VERSION_MAJOR)
	throw SessionError("Computation sent invalid registration data");
}

void LocalSession::stop()
{
    if (mProcess) {
	mTerminationExpected = true;
	mProcess->terminate(false);
    }
}
  
void LocalSession::pause()
{
    if (mProcess) {
        mProcess->signal(SIGSTOP,true);
    }
}

void LocalSession::resume()
{
    if (mProcess) {
        mProcess->signal(SIGCONT,true);
    }
}

// ProcesObserver
void LocalSession::onTerminate(const api::UUID& id,
			       const api::UUID& sessionId,
			       impl::ExitStatus status)
{
    ARRAS_DEBUG("onTerminate called for id: " << id.toString());
    if (id != mAddress.computation ||
	sessionId != mAddress.session)
    {
	ARRAS_ERROR(log::Id("OnTerminateBadId") <<
		    log::Session(mAddress.session.toString()) <<
		    "Incorrect computation or session id passed to onTerminate");
	return;
    }
     
    std::string typeStr = "fail";
    if (status.exitType == impl::ExitType::Exit) typeStr = "exit";
    else if (status.exitType == impl::ExitType::Signal) typeStr = "signal";
    ARRAS_ATHENA_TRACE(0,log::Session(sessionId.toString()) <<
		       "{trace:comp} " << typeStr << " " << id.toString() <<
		       " " << status.status);

    std::unique_lock<std::mutex> lock(mCallbackMutex);
    if (mTerminateCallback) {
	status.convertHighExitToSignal();
	std::string reason = "compExited: " + mName + " " + exitStatusString(status,mTerminationExpected);
	api::Object disconnectStatus;
	disconnectStatus["disconnectReason"] = reason;
	disconnectStatus["execStatus"] = "stopped";
	disconnectStatus["execStoppedReason"] = reason;
	mTerminateCallback(mTerminationExpected,disconnectStatus);
    }
}

void LocalSession::onSpawn(const api::UUID&,
			   const api::UUID&,
			   pid_t)
{
}

}
}
