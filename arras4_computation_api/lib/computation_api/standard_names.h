// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef ARRAS4_STANDARD_NAMES_H_
#define ARRAS4_STANDARD_NAMES_H_

#include <string>

namespace arras4 {
    namespace api {

// names of standard config entries, passed to Computation::configure
struct ConfigNames {
    // name of the computation dso
    static const std::string dsoName;
    // maximum memory that a computation should use, in megabytes
    static const std::string maxMemoryMB;
    // maximum number of threads that a computation should spawn
    static const std::string maxThreads;
};

// standard properties that computations may provide by implementing
// Computation::property()
struct PropNames {
    // define as 'true' to enable hyperthreading (i.e. maxThreads > maxCores)
     static const std::string wantsHyperthreading;
};

// standard environment variables that computations may query by calling
// Computation::environment and/or set via Computation::setEnvironment()
struct EnvNames {
    // returns current computation API version string
    // e.g."4.0.0"
    static const std::string apiVersion; // queryable, not settable
    static const std::string computationName; // queryable, not settable
};


}
}
#endif
