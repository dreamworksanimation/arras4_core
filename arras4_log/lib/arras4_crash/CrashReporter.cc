// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "CrashReporter.h"

// boost::filesystem 3 is required to create temporary directories
#undef BOOST_FILESYSTEM_VERSION
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <common/linux/linux_libc_support.h>    // Google Breakpad
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

namespace {

static bool sCrashReporterEnabled = (getenv("ARRAS_CRASH_REPORT_ENABLE") != NULL) ?
                               static_cast<bool>(atoi(getenv("ARRAS_CRASH_REPORT_ENABLE"))): true;

std::string 
getEnv(const char *aName, const char *aFallbackName = "", const char *aDefaultResult = "unknown")
{
    const char *res = getenv(aName);
    if (!res) {
        res = getenv(aFallbackName);
        if (!res) {
            res = aDefaultResult;
        }
    }
    return res;
}

std::string
getTimeAsString(time_t aRawTime, const char * aFormat)
{
    struct tm * timeinfo;
    char buffer[80];
    timeinfo = localtime(&aRawTime);
    strftime(buffer,80,aFormat,timeinfo);
    return std::string(buffer);
}

std::string 
getStudioEnv()
{
    const char *studioEnv = getenv("STUDIO");
    if (studioEnv) {
        std::string studio(studioEnv);
        boost::algorithm::to_lower(studio);
        if ((studio == "gld") || (studio == "rwc")) {
            return studio;
        }
    }
    return "";
}

// returns tmp directory path string, defaults to "/tmp/"
std::string 
getTmpDir()
{
    try {
        const std::string& alt_dir =  
            boost::filesystem::temp_directory_path().string() +
            boost::filesystem::path::preferred_separator;
        return alt_dir;
    } catch (const std::exception& e) {
        // error when TMPDIR, TMP, TEMP, TEMPDIR with invalid value
        std::cout << "Warning: CrashReporter:getTmpDir error : " << e.what() 
            << ", defaults to /tmp/ \n";
        return "/tmp/";   // use default
    }
}

}

namespace arras4 {
namespace crash {

// alternative stacktrack director when normal path is not accessible
const std::string ALT_DIR = getTmpDir();

CrashReporter::CrashReporter(
        const std::string & aAppName,
        const std::string & aRootInstallPath,
        const std::string & aSessionId,
        const std::string & aCompName,
        const std::string & aDsoName)
    : mAppName(aAppName)    // application that was running
    , mInstallDir(aRootInstallPath)
    , mSessionId(aSessionId)
    , mCompName(aCompName)
    , mDsoName(aDsoName)
{
    if (sCrashReporterEnabled) {
        // generate the filepath we are going to write the stacktrace at.
        // example:
        //   "/studio/common/log-gld/arras/2013/2013.06.26/<user>/
        //                        2013.06.26_09.39.56.<sessionId>.<compName>.stacktrace.txt"
        // note: the .txt extension is added to be able to view in emails more easily.
        const std::string& user = getEnv("USER", "USERNAME");
        mStackTraceFilename = generateDirAndFilename(user, mSessionId, mCompName);

        // Create directory if it doesn't exist
        boost::filesystem::path p(mStackTraceFilename);
        if (p.has_parent_path()) {

            // Normal user permission is good here.  Unless we run out of disk space or
            // the owner of this process does not have permission then
            // creating the stack trace directory should be no problem.
            try {
                boost::filesystem::create_directories(p.parent_path());
            } catch (const boost::filesystem::filesystem_error& e) {
                // can't create, try using the alternate dir
                std::cout << "Warning: CrashReporter failed to create dir: " << p.parent_path()
                          << "; error_code: " << e.code() << ", try using " << ALT_DIR << p.filename().string() << "\n";
                mStackTraceFilename = ALT_DIR + p.filename().string();
                p = boost::filesystem::path(mStackTraceFilename);
            }
        }

        // check access
        if (access(p.parent_path().string().c_str(), R_OK|W_OK|X_OK) != 0) {
            // no access, try alternate dir
            std::cout << "Warning: CrashReporter has no access to " << p.parent_path()
                      << ", try using " << ALT_DIR << p.filename().string() << "\n";
            mStackTraceFilename = ALT_DIR + p.filename().string();
            p = boost::filesystem::path(mStackTraceFilename);
        }

        // set permission for parent directories
        boost::filesystem::path parent = p.parent_path();
        try {
            boost::filesystem::permissions(parent, boost::filesystem::all_all);
        } catch (const boost::filesystem::filesystem_error& e) {
            std::cout << "Warning: CrashReporter failed to set permission for " << parent
                      << "; error_code: " << e.code() << "\n";
        }
        try {
            parent = parent.parent_path();
            boost::filesystem::permissions(parent, boost::filesystem::all_all);
        } catch (const boost::filesystem::filesystem_error& e) {
            std::cout << "Warning: CrashReporter failed to set permission for " << parent
                      << "; error_code: " << e.code() << "\n";
        }

        // need a temporary location to store the minidumps: /tmp/breakpad
        // note that breakpad will already generate the minidump with a unique path
        boost::filesystem::path tempPath(getTmpDir());
        tempPath /= "breakpad";
        mMinidumpDirectory = tempPath.string();    // path to the minidump file

        // we will store the .dmp files in /tmp/breakpad
        // note: the directory where the minidump file goes must be created here.
        // if not, the minidump file won't be created, stack trace will be empty
        boost::filesystem::path dir(mMinidumpDirectory);
        boost::filesystem::create_directories(dir);

        // Google BreakPad Exception Handler
        google_breakpad::MinidumpDescriptor descriptor(minidumpDirectory()); // store minidumps here
        mExceptionHandler.reset(new google_breakpad::ExceptionHandler(descriptor,
                                                                      NULL,
                                                                      CrashReporter::dumpCallback,
                                                                      this, // pass necessary info (appName, .dmp, stacktrace path)
                                                                      true,
                                                                      -1));
    }
}

bool
CrashReporter::dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                            void *context,
                            bool success)
{
    // |success| indicates whether or not the minidump was written. If the minidump
    // has not been written, no need to fork since we have no minidump to process.
    if (!success) {
        return false;   // return false so Breakpad treats exception as unhandled.
    }

    pid_t pid = vfork();
    // Breakpad says the safest thing to do after the application has crashed is to "fork
    // and exec" a new process to do any work you need to do, since the application is in
    // an unstable state.  So, vfork to create a new process here to process the minidump and
    // generate, store, and email the stack trace.  In addition, avoid allocating any memory
    // onto the heap with use of the STL, and instead use Breakpad's reimplementation of
    // libc functions.

    if (pid == 0) {
        CrashReporter *breakpad = (CrashReporter *)(context);

        // argument 1 to the python script, the path to the minidump file, at descriptor.path()
        // argument 2 to python script, the path to the {INSTALL_DIR}data/symbols directory
        // argument 3 to the python script, the application that was running, at breakpad->mApplication
        // argument 4 to the python script, where we will store the stack trace, breakpad->mStackTraceFilename
        // argument 5 to python script, the Arras sessionId if available
        // argument 6 to python script, the Arras computation name if available
        // argument 7 to python script, the computation's dso name if applicable

        // call "python {breakpadProcess} {.dmp} {/symbols} {application} {stackTracePath}"
        // need to construct a string without using STL (we can't trust other libraries since
        // unstable after a crash).  So, use Breakbad's reimplementation of libc's strlcat
        // function to generate the string so we make the system call.  By generating an array
        // of chars of a known size, we avoid allocating memory on the heap, instead we store
        // it on the stack.
        // Note: my_strlscpy and my_strlcat are Breakpad's reimplementations of libc functions

        char command[1024];  // note that this string may be more than 256 characters long.

        // first, add "python"
        my_strlcpy(command, "python ", sizeof(command));

        // then, add path to the script we want to run: {INSTALL_DIR}/bin/breakpadProcess
        my_strlcat(command, breakpad->installDir().c_str(), sizeof(command));
        my_strlcat(command, "/bin/breakpadProcess", sizeof(command));

        // argument 1 to the python script, the path to the minidump file
        my_strlcat(command, " ", sizeof(command));
        my_strlcat(command, descriptor.path() , sizeof(command));

        // argument 2 to python script, the path to the {INSTALL_DIR}/data/symbols directory
        my_strlcat(command, " ", sizeof(command));
        my_strlcat(command, breakpad->installDir().c_str(), sizeof(command));
        my_strlcat(command, "/data/symbols", sizeof(command));

        //argument 3 to the python script, the application that was running
        my_strlcat(command, " ", sizeof(command));
        my_strlcat(command, breakpad->applicationName().c_str(), sizeof(command));

        //argument 4 to the python script, the exact location where we will store the stack trace
        // in /studio/common/log.
        my_strlcat(command, " ", sizeof(command));
        my_strlcat(command, breakpad->stackTraceFilename().c_str(), sizeof(command));

        if (!breakpad->sessionId().empty()) {
            my_strlcat(command, " ", sizeof(command));
            my_strlcat(command, breakpad->sessionId().c_str(), sizeof(command));
        }
        if (!breakpad->compName().empty()) {
            my_strlcat(command, " ", sizeof(command));
            my_strlcat(command, breakpad->compName().c_str(), sizeof(command));
        }
        if (!breakpad->dsoName().empty()) {
            my_strlcat(command, " ", sizeof(command));
            my_strlcat(command, breakpad->dsoName().c_str(), sizeof(command));
        }

        // make the call "python {breakpadProcess} {.dmp} {/symbols} {application} {stackTracePath}"
        system(command); // note that the system() call is implemented useing fork() and exec()
                         // so a new process will be created to do this work, as desired.
        _exit(0);    // terminate the child process immediately
    }

    return false;  // returning false tells Breakpad to treat the exception as unhandled, allowing
                   // another exception handler (usage_log) the chance to handle the exception
}

void
CrashReporter::writeMinidumpFromException(const std::exception &e)
{
    writeUnhandledExceptionWarning(e.what());
}

void
CrashReporter::writeUnhandledExceptionWarning(const std::string &aExceptionName) {
    std::ofstream stacktraceFile;
    stacktraceFile.open(mStackTraceFilename.c_str());
    stacktraceFile << "-----------------------------------------------------------------------" << std::endl;
    stacktraceFile << "UNHANDLED EXCEPTION: " << aExceptionName << std::endl;
    stacktraceFile << "Caught an unhandled exception in notify(reciever, event) in main.cc." << std::endl;
    stacktraceFile << "Unfortunately this means that the following stacktrace cannot provide  " << std::endl;
    stacktraceFile << "much information about where the crash actually occurred." << std::endl;
    stacktraceFile << "-----------------------------------------------------------------------" << std::endl;
    stacktraceFile.close();
    if (mExceptionHandler.get()) {
        mExceptionHandler->WriteMinidump();
    }
}


std::string
CrashReporter::baseDir(const std::string& userId)
{
    // example : /studio/common/log-gld/arras/2018/2018.10.23/<userId>/
    time_t rawtime;
    time(&rawtime);
    const std::string datePortion = getTimeAsString(rawtime, "%Y/%Y.%m.%d/");

    std::string dir("/studio/common/");
    const std::string & studioEnv = getStudioEnv();
    dir += studioEnv.empty() ? "log/arras/" : ("log-" + studioEnv + "/arras/");
    dir += datePortion + userId + "/";
    return dir;
}

std::string
CrashReporter::generateDirAndFilename(const std::string& userId,
                                      const std::string& sessionId,
                                      const std::string& compName)
{
    // example:
    //  /studio/common/log-gld/arras/2018/2018.12.13/prramsey/
    //     2018.12.13_09.21.40.119d93e9-1dca-b438-c0e5-c38ac2e4c92a.mcrt.stacktrace.txt
    // note: add the .txt extension to be able to view in emails more easily.

    time_t rawtime;
    time(&rawtime);

    //const std::string dir = studioCommon + campus + arras + datePortion + user + "/";
    const std::string& dir = baseDir(userId);

    // File name
    const std::string& date = getTimeAsString(rawtime, "%Y.%m.%d_%H.%M.%S");
    std::stringstream ss;

    // add sessionId and compName if available
    if (!sessionId.empty()) ss << "." << sessionId;

    if (!compName.empty()) ss << "." << compName;

    // if no sessionId or compName, use process pid
    if (sessionId.empty() || compName.empty()) {
        ss << "." << getpid();
    }

    return dir + date + ss.str() + ".stacktrace.txt";
}

std::string
CrashReporter::findCrashFile(const std::string& baseDir,
                             const std::string& sessionId,
                             const std::string& compName)
{
    if (baseDir.empty() || sessionId.empty() || compName.empty()) return "";

    boost::filesystem::path dir(baseDir);
    bool ok = false;
    try {
        ok = boost::filesystem::is_directory(dir);
        // check access
        if (ok) {
            ok = access(dir.string().c_str(), R_OK|X_OK) == 0;
        }
    } catch (const boost::filesystem::filesystem_error& /*e*/) {
        // no access/permission to baseDir, try alternate dir
    }

    if (!ok) {
        // no access/permission to baseDir, try alternate dir,
        dir = boost::filesystem::path(ALT_DIR);
    }

    // look in the directory for the filename with sessionId and compName
    const std::string& pattern = sessionId + "." + compName + ".";
    boost::filesystem::directory_iterator end_itr;
    for (boost::filesystem::directory_iterator itr(dir); itr != end_itr; ++itr) {
        if (boost::filesystem::is_regular_file(itr->path())) {
            const std::string& filename = itr->path().string();
            if (filename.find(pattern) != std::string::npos &&
                boost::algorithm::ends_with(filename, ".stacktrace.txt")) {
                return filename;
            }  
        }
    }
    return "";
}

std::string
CrashReporter::getCrashSnippet(const std::string& breakpadTraceFilename)
{
    if (breakpadTraceFilename.empty()) return "";

    // expecting a breakpad stack trace file
    std::ifstream file;
    std::string line;
    std::string results;
    bool found = false;

    file.open(breakpadTraceFilename.c_str());
    for(unsigned int curLine = 0; file.good() && getline(file, line); curLine++) {
       
        // look for the crashed thread
        if (!found &&
            boost::algorithm::starts_with(line, "Thread ") &&
            boost::algorithm::ends_with(line, " (crashed)")) {
            found = true;   // found the block
            results += line + "\n";
            continue;
        }

        if (found && line.size() == 0) break;   // end of the block

        // looking for : 
        if (found) {
            try {
                const std::string& subStr = line.substr(0, 4);
                if (std::stoi(subStr) >= 0) {
                    results += line  + "\n";
                }
            } catch (...) {
            }
        } 
    }
    file.close();
    return results;
}

}
}

