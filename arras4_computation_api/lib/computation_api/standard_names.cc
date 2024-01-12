// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "standard_names.h"

namespace arras4 {
    namespace api {

// Standard config entries, properties and environment variables

const std::string ConfigNames::dsoName           = "dsoName"; 
const std::string ConfigNames::maxThreads        = "limits.maxThreads";        
const std::string ConfigNames::maxMemoryMB       = "limits.maxMemoryMB"; 

const std::string PropNames::wantsHyperthreading = "arras.wantsHyperthreading";

const std::string EnvNames::apiVersion           = "arras.apiVersion";
const std::string EnvNames::computationName      = "computation.name";
}
}
