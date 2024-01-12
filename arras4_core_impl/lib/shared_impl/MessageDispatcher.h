// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGE_DISPATCHERH__
#define __ARRAS4_MESSAGE_DISPATCHERH__

#include "MessageQueue.h"
#include "MessageHandler.h"
#include "ExecutionLimits.h"
#include "DispatcherExitReason.h"

#include <network/network_types.h>
#include <message_api/messageapi_types.h>
#include <message_impl/MessageEndpoint.h>

#include <thread>
#include <exception>
#include <atomic>
#include <chrono>
#include <mutex>


namespace arras4 {
    namespace impl {

// MessageDispatcher runs threads to send and receive messages
// on a MessageEndpoint as quickly as possible. The MessageEndpoint
// is essentially a network socket, although the MessageEndpoint object 
// passed in may contain some extra processing for priority or control messages. 
// For example. if the MessageEndpoint gets a 'stop' message, it may use it to
// stop the dispatcher immediately rather than passing it on to the dispatcher as
// a regular message.
//
// Incoming messages from the MessageEndpoint are routed by the dispatcher to
// a MessageHandler object. Typically this will immediately pass them into a
// Computation's onMessage() handler function. 
//
// Outgoing messages are sent by calling send() on the MessageDispatcher, which 
// will then send them out through the MessageEndpoint. Typically, send() is
// the result of a Computation's request to send a message out.
//
// Sending and receiving through the endpoint (i.e. socket) is buffered using
// two message queues and three threads. This allows a computation to run at
// its own pace, independently of transmission on the socket. The MessageDispatcher
// 'handler' thread, which calls 'onMessage' and 'onIdle', is effectively the main
// thread of the Computation.
//
// When the incoming queue is empty and a message isn't being processed then the
// 'handler' thread is idle. The dispatcher can call the Computation's onIdle()
// function at regular intervals during such periods. The interval is defined
// by 'idleIntervalMicroseconds' passed into the dispatcher constructor. It is
// measured from when the last 'onMessage' or 'onIdle' function returned, so taking
// longer than this time in onIdle will not displace message handling. 
// Passing in zero (or NO_IDLE) for 'idleInterval' prevents idle callback altogether.
//
// Note: The dispatch queues are unbounded, which means if the send rate
// is too high, or handle rate is too low, over a sustained period, then
// transmission delay will grow indefinitely, together with the queue size.
// There is an open task to allow these queues to be bounded by some measure
// (count or total queued message size...)

class DispatcherObserver 
{
public:
    virtual ~DispatcherObserver() {}
    virtual void onDispatcherExit(DispatcherExitReason reason)=0;
};

class MessageDispatcher
{
public:

    static std::chrono::microseconds NO_IDLE; 

    MessageDispatcher(const std::string& label,  // helps debugging
                      MessageHandler& aHandler,
                      const std::chrono::microseconds& idleInterval = NO_IDLE,
                      DispatcherObserver* observer = nullptr)
        : mLabel(label),
          mHandler(aHandler),
          mIdleInterval(idleInterval),
          mObserver(observer),
          mOutgoingQueue(label+":outgoing"),
          mIncomingQueue(label+":incoming"),
          mSentCount(0),
          mReceivedCount(0),
          mState(DispatcherState::NotStarted)
        {}

    ~MessageDispatcher();

    // Place a message on the outgoing queue. Can be called any time after 
    // construction. It will be sent as soon as possible, once startDispatching() 
    // has called.
    bool send(const Envelope& message);

    // startQueuing() begins running the reader thread, so that incoming
    // messages are captured but not handled yet. The MessageEndpoint*
    // passed in as 'aSource' must remain valid until waitForExit() terminates,
    // since reader and writer threads access it.
    //
    // startDispatching() begins running the writer and handler threads,
    // so that pending and subsequent incoming messages are handled, and
    // pending outgoing messages are sent out. ExecutionLimits are applied 
    // to the handler thread : this being the parent of any Computation 
    // work threads
    //
    // waitForExit() blocks until the dispatcher encounters an error, or
    // postQuit() is called from another thread. The return code shows what
    // happened, and is one of ER_Quit, ER_Disconnected, ER_MessageError, or
    // ER_HandlerError.
    //
    // postQuit() can be called from another thread to ask the dispatcher to
    // exit promptly. Usually the exit reason will then be ER_Quit, although
    // errors and quit requests that are almost concurrent may be ambiguous.
    void startQueueing(const std::shared_ptr<MessageEndpoint>& aSource);    
    void startDispatching(const ExecutionLimits& limits = ExecutionLimits());    
    DispatcherExitReason waitForExit(); 
    void postError(DispatcherExitReason reason,const std::string& msg = std::string());
    void postQuit();

    // return counts of sent and received messages. May be called from
    // any thread.
    unsigned long sentMessageCount() const { return mSentCount; }
    unsigned long receivedMessageCount() const { return mReceivedCount; }

private:
    void incomingThreadProc();
    void outgoingThreadProc();
    void handlerThreadProc();
    void masterThreadProc();

    std::string mLabel;
    std::shared_ptr<MessageEndpoint> mSource; 
    ExecutionLimits mLimits;

    MessageHandler& mHandler;
    std::chrono::microseconds mIdleInterval;
    DispatcherObserver* mObserver;

    MessageQueue mOutgoingQueue;
    MessageQueue mIncomingQueue;

    std::thread mMasterThread;

    std::atomic<DispatcherExitReason> mExitReason;

    std::atomic<unsigned long long> mSentCount;
    std::atomic<unsigned long long> mReceivedCount;

    enum class DispatcherState { NotStarted,Queueing,Dispatching,Exiting,Exited };
    std::atomic<DispatcherState> mState;
    std::mutex mStateMutex;
    std::condition_variable mStateCondition;
};

}
}
#endif
