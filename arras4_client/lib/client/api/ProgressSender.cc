// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "ProgressSender.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>
#include <network/IPCSocketPeer.h>

#include <shared_impl/ThreadsafeQueue_impl.h>
#include <execute/SpawnArgs.h>

namespace arras4 {
    namespace impl {	
	template class ThreadsafeQueue<api::Object>;
    }
}

namespace arras4 {
    namespace client {

ProgressSender::~ProgressSender()
{
    stopSending();
}
   
void ProgressSender::setAutoExecCmd(const std::string& cmd)
{
    mAutoExecCmd = cmd;
}

void ProgressSender::setChannel(const std::string& channel)
{
    if (channel.empty()) 
	mAddress.clear();
    else
	mAddress = "/tmp/arrasprog_"+channel+".ipc";
}

void ProgressSender::progress(api::ObjectConstRef message)
{
    if (isDisabled()) return;

    try {
	mQueue.push(message);
    } catch (std::exception& e) {
	ARRAS_INFO("Failed to push progress message: " << e.what());
    }
    startSending();
}
 
void ProgressSender::startSending()
{
    std::unique_lock<std::mutex> lock(mThreadMutex);
    if (!mIsSending && mRun) {
	mSendThread = std::thread(&ProgressSender::sendProc,this);
	mIsSending = true;
    }
}

void ProgressSender::stopSending()
{
    std::unique_lock<std::mutex> lock(mThreadMutex);
    mRun = false;
    if (mIsSending) {
	mStopCondition.notify_all();
	mQueue.shutdown();
	if (mSendThread.joinable()) 
	    mSendThread.join();
	mIsSending = false;
    }
}

namespace {
    bool tryConnect(network::IPCSocketPeer& peer,
	            const std::string& address)
    {
	try {
	    peer.connect(address);
	}
	catch (std::exception&) {
	    return false;
	}
	return true;
    }
}

void ProgressSender::sendWait(unsigned seconds)
{
    std::unique_lock<std::mutex> lock(mStopMutex);
    mStopCondition.wait_for(lock,std::chrono::seconds(seconds));
}

bool ProgressSender::doAutoExec()
{
    api::UUID processId = api::UUID::generate();
    impl::Process::Ptr pp = mProcessManager.addProcess(processId,
						       "progress_monitor",
						       processId);
    if (!pp) {
        ARRAS_DEBUG("Failed to create progress monitor process object");
        return false;
    }
    impl::SpawnArgs sa;
    sa.program = mAutoExecCmd;
    sa.args.push_back("--address");
    sa.args.push_back(mAddress);
    sa.environment.setFromCurrent();

    ARRAS_DEBUG("Launching progress monitor: " << mAutoExecCmd)
    impl::StateChange sc = pp->spawn(sa);
    if (!impl::StateChange_success(sc)) {
        ARRAS_DEBUG("Failed to spawn progress monitor process");
        return false;
    }
    return true;
}

void ProgressSender::sendProc()
{
    ARRAS_DEBUG("Connecting to progress GUI at " << mAddress);
    network::IPCSocketPeer peer;
    bool ok = tryConnect(peer,mAddress);
    if (!mRun) return;
    if (!ok) { 
	if (!mAutoExecCmd.empty()) {
	    doAutoExec();
	}
	// wait for 5 seconds before retrying
	sendWait(5);
    }
    while (mRun) {

	// retry connection
	if (!ok) {
	    ARRAS_DEBUG("Retry connecting to progress GUI at " << mAddress);
	    ok = tryConnect(peer,mAddress);
	    if (!ok) {
		// wait for 20 seconds before next retry
		sendWait(20);
		continue;
	    }
	}

	// if we arrive here, we are successfully connected
	while (mRun) {

            // pull messages from queue and send them
	    try {
		api::Object msg;
		mQueue.pop(msg);
		std::string smsg = api::objectToString(msg);
		size_t len = smsg.size();
		peer.send_or_throw(&len,sizeof(size_t),"msg size");
		peer.send_or_throw(smsg.data(),len,"message");
	    }
	    catch (std::exception& e) {
		ARRAS_DEBUG("Failed to send to progress GUI at " <<
			    mAddress << " : " << e.what() <<". Will reconnect.");
		sendWait(5);
		ok = false;
		break;
	    }
	}
    }
}
    
}
}
