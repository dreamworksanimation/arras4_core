// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MESSAGEMACROSH__
#define __ARRAS4_MESSAGEMACROSH__

#include "messageapi_types.h"
#include "UUID.h"
#include "ObjectContent.h"
#include "ContentRegistry.h"
#include "DataInStream.h"
#include "DataOutStream.h"

// These macros make it easy to define new ObjectContent subclasses
//
// add this to ObjectContent subclass declarations (header file)
// Example:
// (file: MyContent.h)
//
//      #include <message_api/ContentMacros.h>
//      class MyContent : public ObjectContent {
//      public:
//          ARRAS_CONTENT_CLASS(MyContent, "51ae8bd4-fe26-4c3c-9637-49246e0378dc",3);
//          MyContent();
//          ~MyContent();
//          void serialize(api::DataOutStream& to) const;
//          void deserialize(api::DataInStream& from, unsigned version);
//      };
//
// uuidgen can be used to create the value for the class id
//
// You can get id, version and name via member functions, static member functions
// or static variables (e.g. classId(), CLASS_ID(), ID). The class will be registered
// when static initialization completes.
#define ARRAS_CONTENT_CLASS(c, idstr, ver) \
    static const ::arras4::api::ClassID& CLASS_ID() {   \
        static ::arras4::api::ClassID id(idstr);        \
        return id;                                      \
    }                                                   \
    static unsigned CLASS_VERSION() { return ver; }  \
    static const std::string& CLASS_NAME() {         \
            static std::string name(#c);             \
            return name;                             \
        }                                            \
    const ::arras4::api::ClassID& classId() const    \
        { return CLASS_ID(); }                       \
    unsigned classVersion() const                    \
        { return CLASS_VERSION(); }                  \
    const std::string& defaultRoutingName() const    \
        { return CLASS_NAME(); }                     \
    ::arras4::api::MessageContent::Format format() const    \
    { return ::arras4::api::MessageContent::Object; }       \
    typedef std::shared_ptr<c> Ptr;                  \
    typedef std::shared_ptr<const c> ConstPtr;       \
    static const ::arras4::api::ClassID& ID;         \
    static const unsigned VERSION_NUM;               \
    static const std::string NAME;                   \
    class Register {                                             \
    public: Register() {                                         \
        static ::arras4::api::SimpleContentFactory<c> factory;   \
        ::arras4::api::ContentRegistry::singleton()->            \
              registerFactory(CLASS_ID(),&factory);              \
    }                                                            \
    };                                                           \
    static Register REGISTER;


// add this to ContentObject subclass definitions (source file)
// Example:
// (file: MyObject.cc)
//
// #include "MyObject.h"
//
// ARRAS_CONTENT_IMPL(MyObject);
//
//  MyObject::MyObject()
// { ... }
// MyObject::~MyObject()
// { ... }
// void MyObject::serialize(api::DataOutStream& to) const
// { ... }
// void MyObject::deserialize(api::DataInStream& from, unsigned version)
// { ... }
//
#define ARRAS_CONTENT_IMPL(c) \
    const ::arras4::api::ClassID& c::ID = c::CLASS_ID();    \
    const unsigned c::VERSION_NUM = c::CLASS_VERSION();     \
    const std::string c::NAME(#c);                      \
    c::Register c::REGISTER;


#endif
