// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_ENVIRONMENT_H__
#define __ARRAS4_ENVIRONMENT_H__

#include <message_api/Object.h>

#include <string>
#include <array>
#include <map>

namespace arras4 {
    namespace impl {

class Environment 
{
public:

    // construct empty environment
    Environment();

    // set from a vector of strings VAR=val
    // if duplicate vars occur, first is used
    void setFrom(const std::vector<std::string>& v);

    // set from an OS environment
    // (null terminated array of c strings VAR=val)
    // if duplicate vars occur, first is used
    void setFrom(char const * const * env);
 
    // set from current process environment
    void setFromCurrent();

    // set from an Object
    // will throw an exception if obj is not an object 
    // or has a member that is not null, string, bool, int,
    // uint or real
    void setFrom(api::ObjectConstRef obj);

    // access as a vector of strings
    std::vector<std::string> asVector() const;

    // access as an Object. all members will be string
    // values
    api::Object asObject() const;

    // set a variable. If override is false
    // and name already exists, don't set
    void set(const std::string& name,
	     const std::string& val,
	     bool override = true);
    
    // remove a value
    void remove(const std::string& name);

    // is a value present?
    bool has(const std::string& name) const;

    // get a value
    std::string get(const std::string& name,
		    const std::string& deflt="") const;

    // access map
    const std::map<std::string,std::string>& map() const;

    // merge another environment
    // in case of conflict, variables in other win
    // iff "override" is true
    void merge(const Environment& other, bool override);

    // merge current environment
    void mergeCurrent(bool override);

private:
    void setString(const std::string& s);

    std::map<std::string,std::string> mMap;
};

}
}
#endif
