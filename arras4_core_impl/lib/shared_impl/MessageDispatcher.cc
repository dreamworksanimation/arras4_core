// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MessageDispatcher.h"
#include "DispatcherExitReason.h"

#include <core_messages/ExecutorHeartbeat.h>
#include <exceptions/ShutdownException.h>
#include <message_api/Message.h>
#include <network/PeerException.h>
#include <message_impl/Envelope.h>
#include <message_impl/MessageReader.h>
#include <message_impl/MessageWriter.h>

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

using namespace arras4::network;

namespace arras4 {
    namespace impl {

std::chrono::microseconds MessageDispatcher::NO_IDLE(0);

MessageDispatcher::~MessageDispatcher() 
{
    postQuit();
    waitForExit();
}

bool MessageDispatcher::send(const Envelope& envelope)
{
    bool ok = true;
    try {
        mOutgoingQueue.push(envelope);
    } catch (ShutdownException&) {
        ok = false;
    }  catch (std::exception& e) {
        ARRAS_ERROR(log::Id("exceptionSending") <<
                    "MessageDispatcher [" << mLabel << "] : exception while sending message : " << e.what());
        ok = false;
    } catch (...) {
        ARRAS_ERROR(log::Id("exceptionSending") <<
                    "MessageDispatcher [" << mLabel << "] : Unknown exception while sending message");
        ok = false;
    }
    return ok;
}

void MessageDispatcher::incomingThreadProc()
{
    log::Logger::instance().setThreadName("incoming");
    while (mState != DispatcherState::Exiting) {
        try {
            Envelope envelope = mSource->getEnvelope(); 
                       // mSource is valid while thread is running..
            mIncomingQueue.push(envelope);
        } catch (ShutdownException&) {
            // queue has been unblocked to give us a chance to exit
        } catch (network::PeerDisconnectException&) {
            postError(DispatcherExitReason::Disconnected);
        } catch (std::exception& e) {
            postError(DispatcherExitReason::MessageError,e.what());
        } catch (...) {
            postError(DispatcherExitReason::MessageError);
        }
    }
}

void MessageDispatcher::outgoingThreadProc()
{
    log::Logger::instance().setThreadName("outgoing");
    while (mState != DispatcherState::Exiting) {
        try {
            Envelope envelope;
            mOutgoingQueue.pop(envelope);
            mSource->putEnvelope(envelope);   
                      // mSource is valid while thread is running...
            // Don't count heartbeat in the message count
            if (envelope.classId() != ExecutorHeartbeat::ID) {
                mSentCount++;
            }
        } catch (ShutdownException&) {
            // queue has been unblocked to give us a chance to exit
        } catch (network::PeerDisconnectException&) {
            postError(DispatcherExitReason::Disconnected);
        } catch (std::exception& e) {
            postError(DispatcherExitReason::MessageError,e.what());
        } catch (...) {
            postError(DispatcherExitReason::MessageError);
        }
    }
}

void MessageDispatcher::handlerThreadProc()
{
    log::Logger::instance().setThreadName("handler");
    while (mState != DispatcherState::Exiting) { 
        Envelope envelope;
        try {     
            // handler thread calls onIdle if it waits too long
            // for a message to be ready.
            bool popped = mIncomingQueue.pop(envelope,mIdleInterval);
            if (popped) {
                mReceivedCount++;
                mHandler.handleMessage(envelope.makeMessage());
            }
            else
                mHandler.onIdle();
        } catch (ShutdownException&) {
            // queue has been unblocked to give us a chance to exit
        } catch (std::exception& e) {
            postError(DispatcherExitReason::HandlerError,e.what());
        } catch (...) {
            postError(DispatcherExitReason::HandlerError);
        }
    }
}

void MessageDispatcher::masterThreadProc()
{
    // this thread is started when the function startQueuing is called
    std::unique_lock<std::mutex> lock(mStateMutex);

    // start receiving messages and placing them on the incoming queue
    std::thread incoming(&MessageDispatcher::incomingThreadProc, this);

    // wait for the signal to start dispatching
    while ((mState != DispatcherState::Dispatching) && (mState != DispatcherState::Exiting))
        mStateCondition.wait(lock);

    std::thread outgoing;
    std::thread handler;
    if (mState != DispatcherState::Exiting) {
        // start sending and dispatching messages
        outgoing = std::thread(&MessageDispatcher::outgoingThreadProc, this);
        handler = std::thread(&MessageDispatcher::handlerThreadProc,this);
        mLimits.apply(handler);

        // wait for the signal to exit
        while (mState != DispatcherState::Exiting)
            mStateCondition.wait(lock);
    }

    // make sure all the threads are unblocked
    mIncomingQueue.shutdown();
    mOutgoingQueue.shutdown();
    mSource->shutdown();

    // wait for the threads to exit
    // unlock mutex, in case a thread ends up calling
    // postError or postQuit
    lock.unlock();
    if (incoming.joinable()) incoming.join();
    if (outgoing.joinable()) outgoing.join();
    if (handler.joinable()) handler.join();

    mSource.reset();

    if (mObserver) mObserver->onDispatcherExit(mExitReason);

    lock.lock();
    mState = DispatcherState::Exited;
}


// start the master thread, so that incoming messages are queued
// even though they are not processed yet.
void MessageDispatcher::startQueueing(const std::shared_ptr<MessageEndpoint>& aSource)
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    if (mState != DispatcherState::NotStarted)
        throw std::logic_error("MessageDispatcher [" + mLabel + "] : called startQueueing after dispatcher has started");
        
    mSource = aSource;
    mState = DispatcherState::Queueing;
    mMasterThread = std::thread(&MessageDispatcher::masterThreadProc, this);
}

// signal the master thread to start dispatching
void MessageDispatcher::startDispatching(const ExecutionLimits& limits)
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    if (mState != DispatcherState::Queueing)
        throw std::logic_error("MessageDispatcher [" + mLabel + "] : called startDispatching when not in 'queueing' state");
    mLimits = limits;
    mState = DispatcherState::Dispatching;
    mStateCondition.notify_one();
}

DispatcherExitReason MessageDispatcher::waitForExit()
{
    if (mMasterThread.joinable()) mMasterThread.join();
    return mExitReason;
}

void MessageDispatcher::postError(DispatcherExitReason reason, const std::string& msg) 
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    std::string logmsg = "Message Dispatcher  [" + mLabel + "] : exiting : reason is '" +
        dispatcherExitReasonAsString(reason) + "'";
    if (!msg.empty()) {
        logmsg += ", the exception message was : '" + msg+"'";
    }
    ARRAS_ERROR(log::Id("dispatcherExit") << logmsg);
    mExitReason = reason;
    mState = DispatcherState::Exiting;
    mStateCondition.notify_one();
}
    
void MessageDispatcher::postQuit() 
{
    std::unique_lock<std::mutex> lock(mStateMutex);
    if (mState == DispatcherState::Exiting ||
        mState == DispatcherState::Exited)
        return;
    ARRAS_DEBUG("MessageDispatcher [" << mLabel << "] : Quit requested");
    mExitReason = DispatcherExitReason::Quit;
    mState = DispatcherState::Exiting;
    mStateCondition.notify_one();
}



}
}
