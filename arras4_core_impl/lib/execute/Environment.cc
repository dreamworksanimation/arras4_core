// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "Environment.h"
#include <unistd.h>

#if defined(JSONCPP_VERSION_MAJOR)
#define memberName name
#endif

namespace arras4 {
    namespace impl {

Environment::Environment()
{
}
   
// helper : set from string "VAR=val", but only
// if VAR is not already defined
void Environment::setString(const std::string& s)
{
    std::string::size_type p = s.find('=');
    if (p != std::string::npos) {
	set(s.substr(0,p),
	    s.substr(p + 1,std::string::npos),
	    false);
    }
}

void Environment::setFrom(const std::vector<std::string>& v)
{
    for (const std::string& s : v) {
	setString(s);
    }
}

void Environment::setFrom(char const * const * env)
{
    while (*env) {
	setString(*env);
	env++;
    }
}

void Environment::setFromCurrent()
{
    setFrom(environ);
}

void Environment::setFrom(api::ObjectConstRef obj)
{
    for (api::ObjectConstIterator it = obj.begin();
         it != obj.end(); ++it) {
	set(it.memberName(),(*it).asString());
    }
}

std::vector<std::string> Environment::asVector() const
{
    std::vector<std::string> ret;
    for (const auto& entry : mMap) {
	ret.push_back(entry.first + "=" + entry.second);
    }
    return ret;
}

api::Object Environment::asObject() const
{
    api::Object ret;
    for (const auto& it : mMap) {
	ret[it.first] = it.second;
    }
    return ret;
}

void Environment::set(const std::string& name,
		      const std::string& val,
                      bool override)
{
    if (!override && has(name)) return;
    mMap[name] = val;
}
    
void Environment::remove(const std::string& name)
{
    mMap.erase(name);
}

// is value present?
bool Environment::has(const std::string& name) const
{
    return mMap.count(name) != 0;
}

// get a value
std::string Environment::get(const std::string& name,
			     const std::string& deflt) const
{
    auto it = mMap.find(name);
    if (it == mMap.end()) return deflt;
    return it->second;
}

// merge another enviroment into this one
void Environment::merge(const Environment& other, bool override)
{
    for (const auto& entry : other.mMap) {
	set(entry.first,entry.second,override);
    }
}

void Environment::mergeCurrent(bool override)
{
    Environment c;
    c.setFromCurrent();
    merge(c,override);
}
    
const std::map<std::string,std::string>&  Environment::map() const
{
    return mMap;
}

}
}
