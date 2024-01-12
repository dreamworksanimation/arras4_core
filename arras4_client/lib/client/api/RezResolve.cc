// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "RezResolve.h"
#include "SessionDefinition.h"

#include <execute/RezContext.h>

#if defined(JSONCPP_VERSION_MAJOR)
#define memberName name
#endif

namespace {
    const char* DEFAULT_PACKAGING = "rez1";

    std::string getString(arras4::api::ObjectConstRef obj,
			  const std::string& key,
			  const std::string& def = std::string())
    {
	if (obj.isObject() &&
	    obj[key].isString())
	    return obj[key].asString();
	return def;
    }
}

namespace arras4 {
    namespace client {

// rezSettings contains "rez_packages", and optionally "packaging_system",
// "pseudo-compiler", "rez_packages_prepend". 
// rezResolve removes "rez_packages" and replaces it with "rez_context" set
// to the resolved context for the packages.
// If there is no action to be taken ("rez_packages" is absent or "packaging_system" is
// set to "none") returns the string "ok"
// If resolution is successful, returns the resolved context
// If resolution fails, returns "error" and puts an error message in err
std::string rezResolve(impl::ProcessManager& procMan,
		       api::ObjectRef rezSettings,
                       std::string& err)
{    
    if (!rezSettings.isObject()) {
	err = "Invalid rez settings";
	return "error";
    }

    std::string packagingSystem = getString(rezSettings,"packaging_system",DEFAULT_PACKAGING);
                                     
    if (packagingSystem.empty() || 
        packagingSystem == "none")
        return "ok";

    std::string rezPackages = getString(rezSettings,"rez_packages");
    if (rezPackages.empty())
	return "ok";

    unsigned rezMajor;
    if (packagingSystem == "rez1") {
        rezMajor = 1;
    } else if (packagingSystem == "rez2") {
        rezMajor = 2;
    } else {
	err = std::string("Unknown packaging system '") + packagingSystem + "'";
	return "error";
    }

    std::string pseudoCompiler = getString(rezSettings,"pseudo-compiler");
    std::string rezPathPrefix = getString(rezSettings,"rez_packages_prepend"); 
    try {
	impl::RezContext rc("rez_resolve", 
			    rezMajor, 
			    rezPathPrefix, 
			    false,
			    pseudoCompiler);
        std::string context = rc.resolvePackages(procMan,rezPackages,err);
	if (context.empty())
	    return "error";
	
	rezSettings.removeMember("rez_packages");
	rezSettings["rez_context"] = context;
	return context;
    } catch (std::exception& e) {
	err = std::string("Exception: ") + e.what();
	return "error";
    }
}

// processes a whole definition as above. Returns true if processing
// completes, otherwise false and err is set to the first error encountered
bool rezResolve(impl::ProcessManager& procMan,
		SessionDefinition& def,
		std::string& err)
{
    api::ObjectRef obj = def.getObject();
    if (!obj.isObject() ||
	!obj["computations"].isObject() ||
	obj["computations"].isNull()) {
	err = "Invalid session definition";
	return false;
    }
    api::ObjectRef comps = obj["computations"];
    for (api::ObjectIterator cIt = comps.begin();
         cIt != comps.end(); ++cIt) {
	std::string name = cIt.memberName();
	api::ObjectRef cdef = *cIt;
	if (cdef.isObject()) {
            if (cdef.isMember("requirements")) {
	        std::string res = rezResolve(procMan,
	                                     cdef["requirements"],
	                                     err);
	        if (res == "error") {
		    err = name + ": " + err;
		    return false;
                }
	    }			 
	}
    }
    return true;
}
    
}
}

