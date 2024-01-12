// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef ARRAS_CRASHREPORTER_HAS_BEEN_INCLUDED
#define ARRAS_CRASHREPORTER_HAS_BEEN_INCLUDED

#include <client/linux/handler/exception_handler.h> // Google Breakpad
#include <string>
#include <unistd.h>

/*
 * Macro defined for simple usage
 */

// assuming getProgramParentPath() is '<install_path>/bin'
// and with sessionId defaults to "" and compName defaults to ""
#define INSTANCE_CRASH_REPORTER(APP_NAME) \
    arras4::crash::CrashReporter crashReporter((APP_NAME), arras4::crash::CrashReporter::getProgramParentPath())


/*
 * Macro defined usage for specifying install_path, and compile out code by default.
 * install_path: for symbols and breakpadProcess.
 */

// assuming we can find data/symbols and bin/breakpadProcess in $INSTALL_PATH
#define INSTANCE_CRASH_REPORTER_WITH_INSTALL_PATH(APP_NAME, INSTALL_PATH, SESSION_ID, COMP_NAME, DSO_NAME) \
    arras4::crash::CrashReporter crashReporter((APP_NAME), (INSTALL_PATH), (SESSION_ID), (COMP_NAME), (DSO_NAME))


/**
 * CrashReporter is a crash reporting system that uses Google's Breakpad.
 *
 * An instantiation of this object will set up Breakpad to automatically
 * generate stacktrace in the event of a program crash.  (In conjunction
 * with 'mod/python/studio/app/breakpad/breakpadProcess').
 *
 * To add to your program:
 *     1. #include <common/crash/CrashReporter.h>
 *     2. Add 'common_crash' to SConscript.
 *     3. Add instance of CrashReporter to beginning of 'main()'.
 *          - Use macro:  INSTANCE_CRASH_REPORTER("executable name")
 *
 * To generate symbols for binaries that Breakpad will use for stacktraces:
 *      This is crucial for informative stack traces.
 *
 *      Libraries installed with 'env.DWAInstallLib(...)' in SConscript do not
 *      need this.  However, libs installed with 'env.DWAInstallDso()' do.
 *      Currently, no support for executables exist in the python tool
 *      'site_scons/site_tools/breakpad.py'.
 *      TODO: Add variation 'env.DWAGenerateBreakpadSymbols(...)' for executables.
 *
 *      If using 'env.DWAInstallLib(...)' in SConscript,
 *          1. Add 'env.DWAGenerateBreakpadSymbols(...)'
 *
 * To build your program so that CrashReporter is enabled during runtime:
 *      1. Build with scons option '--use-breakpad'.
 *
 * NOTE: If building without '--use-breakpad', then CrashReporter functionality
 *       is compiled out, and Breakpad is never setup.
 */
namespace arras4 {
namespace crash {

class CrashReporter
{
public:

    CrashReporter(const std::string &aApplication,
    		      const std::string &aRootInstallPath,
				  const std::string &aSessionId = "",
				  const std::string &aCompName = "",
				  const std::string &aDsoName = "");

    const std::string & applicationName() const { return mAppName; }
    const std::string & installDir() const { return mInstallDir; }
    const std::string & sessionId() const { return mSessionId; }
    const std::string & compName() const { return mCompName; }
    const std::string & dsoName() const { return mDsoName; }
    const std::string & stackTraceFilename() const { return mStackTraceFilename; }

    void writeMinidumpFromException(const std::exception &e);

    static std::string getCWDParentPath() {
        char buf[256];
        getcwd(buf, 256);
        std::string path(buf);
        return path.substr(0, path.find_last_of('/'));
    }

    // Get program parent path from "/proc/$pid/exe" sym link:
    // Note: Since cwd is no longer related to the program path,
    //       we are using the unix /proc virtual file system to resolve
    //       our full program path.  We need the full program path for the
    //       breakpadProcess and breakpad symbols.
    static std::string getProgramParentPath() {
        // Read /proc/$pid/exe sym link:
        std::string proc = "/proc/" + std::to_string(getpid()) + "/exe";
        char buf[1000];
        size_t rc = readlink(proc.c_str(), buf, sizeof(buf));
        if (rc > 0) {
            const std::string path(buf, rc);
            // remove the "/bin/..." part to get the parent path
            std::size_t pos = path.rfind("/bin/");
            if (pos != std::string::npos) {
                return path.substr(0, pos);
            }
        }
        return "";
    }

    // stack trace directory path:
    // example : /studio/common/log-gld/arras/2018/2018.10.23/${userId}/
    static std::string baseDir(const std::string& userId);

    // stack trace file path, example:
    // /studio/common/log-gld/arras/2018/2018.10.23/${userId}/
    //     <time-stamp>.${sessionId}.${compName}.stacktrace.txt
    static std::string generateDirAndFilename(const std::string& userId,
                                              const std::string& sessionId="",
                                              const std::string& compName="");
    
    // find crash file in baseDir with sessionId and compName
    static std::string findCrashFile(const std::string& baseDir,
                                     const std::string& sessionId,
                                     const std::string& compName);

    // extract a few lines from the top of the breakpad stack trace file
    static std::string getCrashSnippet(const std::string& breakpadTraceFilename);

private:
    const std::string & minidumpDirectory() const { return mMinidumpDirectory; }

    static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                             void *context,
                             bool success);

    void writeUnhandledExceptionWarning(const std::string &aExceptionName);

    std::string mAppName;
    std::string mStackTraceFilename;
    std::string mMinidumpDirectory;
    std::string mInstallDir;
    std::string mDirName;
    std::string mFileName;
    std::string mSessionId;
    std::string mCompName;
    std::string mDsoName;
    std::unique_ptr<google_breakpad::ExceptionHandler> mExceptionHandler;
};

}
}

#endif

