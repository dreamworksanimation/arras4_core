// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "AutoLogger.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdio.h>
#include <thread>
#include <stdexcept>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <unistd.h>

namespace arras4 {
    namespace log {
 
// captures a std output (i.e. stdout or stderr) and sends it to the logger
class OutputCapture {
  public:
    const size_t MAX_QUEUE=4096;
    const size_t MAX_LOG_LENGTH=4096;
    OutputCapture(FILE* aFile, 
             const std::string& aPrefix, 
             Logger& logger,
             Logger::Level aLogLevel);
    ~OutputCapture();

    // return the duplicate so the capture can be bypassed
    FILE* getDuplicate() {
        return mDuplicatedFile;
    }
  private:
    std::queue<std::string> mQueue;
    std::mutex mQueueMutex;
    std::condition_variable mQueueCondition;

    void pipeThreadFunc();
    void logThreadFunc();

    std::thread mPipeThread;
    std::thread mLogThread;

    // this FILE and fd allows reading from the pipe
    FILE* mPipeFile;
    int mPipeFd;

    // this FILE and fd will write where the original wrote
    FILE* mDuplicatedFile;
    int mDuplicatedFd;

    // this fd is what was originally passed in
    int mOriginalFd;

    // the prefix to add to to each line
    std::string mPrefix;

    // the logger and log level to be used for the redirect
    Logger& mLogger;
    Logger::Level mLogLevel;
};

// To keep the pipe clear this thread is dedicatd to just pulling messages
// out of the pipe and queuing them to be logged by the logger thread.
// This will hopefully keep print to stdout and stderr from blocking
void
OutputCapture::pipeThreadFunc()
{
    char buffer[MAX_LOG_LENGTH];
    while (1) {
        char* strptr = fgets(buffer,static_cast<int>(sizeof(buffer)), mPipeFile);
        if (strptr != NULL) {
            std::string message(strptr);

            {
                std::unique_lock<std::mutex> lock(mQueueMutex);

                // if things have gotten too deep then wait
                while (mQueue.size() >= MAX_QUEUE) mQueueCondition.wait(lock);

                mQueue.push(message);

                // if this is a transition to not empty then
                // wake logger thread
                if (mQueue.size() == 1) mQueueCondition.notify_all();
            }

            // see if this is the last message before Prefixer shutdown
            if (strcmp(buffer, "Closing stream prefixer\n") == 0) break;
        }
    }
}

// This thread just takes strings off of the queue, strips trailing
// carraige returns, and logs them
void
OutputCapture::logThreadFunc()
{
    std::string message;
    while (1) {
        {
            std::unique_lock<std::mutex> lock(mQueueMutex);

            // if the queue is empty then wait for it to be not empty
            while (mQueue.size() == 0) {
                mQueueCondition.wait(lock);
            }

            message= mQueue.front();
            mQueue.pop();


            // if the queue just became not full then wake the pipe thread
            if (mQueue.size() == (MAX_QUEUE-1)) {
                mQueueCondition.notify_all();
            }
        }

        // strip off the carraige return
        size_t length = message.length();
        if ((length > 0) && (message[length - 1] == '\n')) {
            length--;
            message = message.substr(0, length);
        }

        // see if this is the last message before Prefixer shutdown
        if ((length == 23) && (message == "Closing stream prefixer")) {
            // put it back in case there are more logger threads
            std::unique_lock<std::mutex> lock(mQueueMutex);
            mQueue.push(message);
            mQueueCondition.notify_all();
            break;
        }

        // log the message
        mLogger.logMessage(mLogLevel, "%s%s", mPrefix.c_str(), message.c_str());
    }
}

OutputCapture::~OutputCapture()
{
    // send a special string to indicate to stop the thread
    write(mOriginalFd, "\nClosing stream prefixer\n", 25);

    // finish shutting down the threads
    mPipeThread.join();
    mLogThread.join();

    // to put things back we have to assume that nobody else did
    // a redirect
    dup2(mDuplicatedFd, mOriginalFd);

    // no more need for the extra file descriptors
    fclose(mPipeFile);
    fclose(mDuplicatedFile);
}

OutputCapture::OutputCapture(FILE* aFile, 
                             const std::string& aPrefix, 
                             Logger& logger,
                             Logger::Level aLevel) :
    mPrefix(aPrefix),
    mLogger(logger),
    mLogLevel(aLevel)
{
    // create a pipe pair
    int redirectPipe[2];
    if (pipe(redirectPipe) < 0) throw std::runtime_error(std::string("Couldn't create a pipe"));
    mPipeFd = redirectPipe[0];

    mOriginalFd = fileno(aFile);

    // duplicate the original fd
    mDuplicatedFd = dup(mOriginalFd);
    if (mDuplicatedFd < 0) throw std::runtime_error("Couldn't dup the file descriptor");

    // make the original fd into the pipe input
    if (dup2(redirectPipe[1], mOriginalFd) < 0) {
        throw std::runtime_error("Couldn't dup pipe input over original file descriptor");
    }
    // go ahead and close the old copy of the pipe input
    close(redirectPipe[1]);

    // make the duplicate of the original into a stream
    mDuplicatedFile = fdopen(mDuplicatedFd, "w");
    if (mDuplicatedFile == NULL) {
        throw std::runtime_error("Couldn't make the stream for original output");
    }

    // make the pipe output into a stream
    mPipeFile = fdopen(mPipeFd, "r");
    if (mPipeFile == NULL) {
        throw std::runtime_error("Couldn't make the stream for pipe output");
    }

    mPipeThread = std::thread(&OutputCapture::pipeThreadFunc, this);
    mLogThread = std::thread(&OutputCapture::logThreadFunc, this);
}

AutoLogger::AutoLogger(Logger& logger)
    : mLogger(logger)
{
    init();
}

AutoLogger::AutoLogger()
    : mLogger(Logger::instance())
{
    init();
}

void
AutoLogger::init()
{
     // redirect stderr to prefix information
    mStderr = new OutputCapture(stderr, "stderr ", mLogger,Logger::Level::LOG_ERROR);

    // get logging to use the alternate stderr to avoid an infinite loop
    mErrBuf =  new __gnu_cxx::stdio_filebuf<char>(fileno(mStderr->getDuplicate()), std::ios::out);
    mErrStream = new std::ostream(mErrBuf);
    mLogger.setErrStream(mErrStream);

    mStdout = new OutputCapture(stdout, "stdout ", mLogger, Logger::Level::LOG_INFO);
    mOutBuf =  new __gnu_cxx::stdio_filebuf<char>(fileno(mStdout->getDuplicate()), std::ios::out);
    mOutStream = new std::ostream(mOutBuf);
    mLogger.setOutStream(mOutStream);

    // don't buffer the output
    std::cout.setf(std::ios_base::unitbuf);
}

AutoLogger::~AutoLogger()
{
    // flush the buffered output before turning off redirect
    fflush(stdout);
    std::cout << std::flush;

    // clean up the redirects
    // stop redirecting stderr to the pipe
    delete mStderr;

    // switch the logging back to cerr
    mLogger.setErrStream(&std::cerr);

    // delete the ostream and FILE*
    delete mErrStream;
    delete mErrBuf;

    delete mStdout;
    mLogger.setOutStream(&std::cout);
    delete mOutStream;
    delete mOutBuf;
}
    
}
}
