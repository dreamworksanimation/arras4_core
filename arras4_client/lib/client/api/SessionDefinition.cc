// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "SessionDefinition.h"

#include <arras4_log/Logger.h>
#include <arras4_log/LogEventStream.h>

#include <cstdlib>
#include <fstream>
#include <unistd.h>



namespace arras4 {
    namespace client {

namespace {

constexpr const char* ENV_VAR = "ARRAS_SESSION_PATH";
constexpr const char* ENV_CONTEXTS = "ENV_CONTEXTS";

// Client override environment variables
// if these are set, they override normal client behavior
const char* ENV_OVR_SESSION_DEF_SUFFIX = "ARRASCLIENT_OVR_SESSION_DEF_SUFFIX";

std::string findFile(const std::string& filename,
                     const std::string& filepath)
{ 
    size_t start = 0;
    while (true) {
        std::size_t end = filepath.find_first_of(':',start);
        bool done = (end == std::string::npos);
        
        if (end > start+1) {
     
            std::string directory = filepath.substr(start, done ? end : end-start);

            std::string path = directory + '/' + filename;
            if (access(path.c_str(), R_OK) != -1) {
                return path;
            }
        }

        if (done)
            return std::string();

        start = end+1;
    }
}

void getContextsFromEnvironment(api::ObjectRef out)
{
    const char* ec = getenv(ENV_CONTEXTS);
    if (!ec) return;
    std::ifstream ifs(ec);
    if (ifs.fail()) {
	throw DefinitionAttachError(std::string("Couldn't open env contexts file '") + ec +"'");
    }
    try {
	ifs >> out;
    } catch (std::exception& e) {
	throw DefinitionAttachError(std::string("Couldn't load env contexts file '") + ec +"': "+e.what());
    }
}

} // namespace {

SessionDefinition::SessionDefinition(const std::string& filepath)
{
    loadFromFile(filepath);
}

SessionDefinition::SessionDefinition(api::ObjectConstRef object) : mObject(object)
{
}

/* static */
SessionDefinition SessionDefinition::load(const std::string& name,
                                          const std::string& searchPath)
{
    if (searchPath.empty()) {
        throw DefinitionLoadError(std::string("Session definition search path is empty. "
                                              "Try setting the environment variable ")
                                  +ENV_VAR);
    }
 
    std::string filepath;

    // debug override
    const char* suffix = getenv(ENV_OVR_SESSION_DEF_SUFFIX);
    if (suffix) {
	filepath = findFile(name + suffix + ".sessiondef", searchPath);
	if (!filepath.empty()) {
	    ARRAS_WARN(log::Id("ClientOverrideActive") <<
		       "Overriding session: " << name << " to " << (name + suffix));
	}
    }
    if (filepath.empty()) {
	filepath = findFile(name + ".sessiondef", searchPath);
    }
  
    if (filepath.empty())
        throw DefinitionLoadError(std::string("Couldn't find definition file for '") + name +"' in search path");
    return SessionDefinition(filepath);
}

/* static */
std::string SessionDefinition::getDefaultSearchPath()
{
    static std::string searchPath;
    if (searchPath.empty()) {
        char* sp = std::getenv(ENV_VAR);
        if (sp)
            searchPath = sp;
    }
    return searchPath;
}
    
void SessionDefinition::loadFromFile(const std::string& filepath)
{
     std::ifstream ifs(filepath);
     if (ifs.fail()) {
         throw DefinitionLoadError(std::string("Couldn't open definition file '") + filepath +"'");
     }
     try {
         ifs >> mObject;
     } catch (std::exception& e) {
         throw DefinitionLoadError(std::string("Couldn't load definition file '") + filepath +"': "+e.what());
     }
}

void SessionDefinition::saveToFile(const std::string& filepath)
{
     std::ofstream ofs(filepath);
     if (ofs.fail()) {
         throw DefinitionSaveError(std::string("Couldn't save definition file '") + filepath +"'");
     }
     try {
         ofs << mObject;
     } catch (std::exception& e) {
         throw DefinitionSaveError(std::string("Couldn't save definition file '") + filepath +"': "+e.what());
     }
}

void SessionDefinition::attachContextObject(const std::string& name, api::ObjectConstRef obj)
{
    mObject["contexts"][name] = obj;
}
    
bool SessionDefinition::attachContext(const std::string& name)
{
    api::Object ctxs;
    getContextsFromEnvironment(ctxs);
    if (ctxs[name].isNull()) {
	return false;
    } else {
	attachContextObject(name,ctxs[name]);
	return true;
    }
}
   
/* static */
bool SessionDefinition::isContextInEnvironment(const std::string& name)
{ 
    api::Object ctxs;
    getContextsFromEnvironment(ctxs);
    return !ctxs[name].isNull();
}

bool SessionDefinition::isContextDefined(const std::string& name) const
{
    api::ObjectConstRef ctxt = mObject["contexts"][name];
    if (ctxt.isNull() || !ctxt.isObject())
	return false;
    return true;
}

bool SessionDefinition::checkNamedContexts() const
{ 
    api::ObjectConstRef comps = mObject["computations"];
    for (api::ObjectConstIterator cIt = comps.begin();
         cIt != comps.end(); ++cIt) {
	api::ObjectConstRef cdef = *cIt;
	if (cdef.isObject()) {
	    api::ObjectConstRef reqs = cdef["requirements"];
            if (reqs.isObject() &&
		reqs["context"].isString()) {
	        std::string ctxName = reqs["context"].asString();
		if (!isContextDefined(ctxName))
		    return false;
	    }
	}			 
    }
    return true;
}

}
}
