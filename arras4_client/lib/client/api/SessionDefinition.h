// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_SESSION_DEFINITION_H__
#define __ARRAS4_SESSION_DEFINITION_H__

#include <message_api/Object.h>
#include <exception>

namespace arras4 {
    namespace client {

class DefinitionLoadError : public std::exception
{
    public:
   
    DefinitionLoadError(const std::string& msg) : mWhat(msg) {}
    ~DefinitionLoadError() throw() {}

    const char* what() const throw() { return mWhat.c_str(); }

protected:
    std::string mWhat;
};

class DefinitionSaveError : public std::exception
{
    public:
   
    DefinitionSaveError(const std::string& msg) : mWhat(msg) {}
    ~DefinitionSaveError() throw() {}

    const char* what() const throw() { return mWhat.c_str(); }

protected:
    std::string mWhat;
};

class DefinitionAttachError : public std::exception
{
    public:
   
    DefinitionAttachError(const std::string& msg) : mWhat(msg) {}
    ~DefinitionAttachError() throw() {}

    const char* what() const throw() { return mWhat.c_str(); }

protected:
    std::string mWhat;
};

class SessionDefinition
{
public:
    SessionDefinition() {}
    SessionDefinition(const std::string& filepath);
    SessionDefinition(api::ObjectConstRef object);

    void loadFromFile(const std::string& filepath);
    static SessionDefinition load(const std::string& name,
                                  const std::string& searchPath = getDefaultSearchPath());

    void saveToFile(const std::string& filepath);

    api::ObjectConstRef getObject() const { return mObject; }
    api::ObjectRef getObject() { return mObject; }

    api::ObjectRef operator[](const std::string& compName) 
        { return mObject["computations"][compName]; }
    api::ObjectConstRef operator[](const std::string& compName) const
        { return mObject["computations"][compName]; }
    bool has(const std::string& compName) const
        { return mObject["computations"].isMember(compName); }

    // these may throw DefinitionAttachError
    void attachContextObject(const std::string& name, api::ObjectConstRef obj);
    bool attachContext(const std::string& name);
    static bool isContextInEnvironment(const std::string& name);
    
    bool isContextDefined(const std::string& name) const;
    bool checkNamedContexts() const;

protected:
    static std::string getDefaultSearchPath();

private:
    api::Object mObject;
};

}
}
#endif


    
