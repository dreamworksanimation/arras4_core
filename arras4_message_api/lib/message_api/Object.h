// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_OBJECTH__
#define __ARRAS4_OBJECTH__

#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>

// These typedefs define the representation of a 'dynamic untyped value'
// Arras uses jsoncpp to represent these values.
namespace arras4 {
    namespace api {
        
        typedef Json::Value Object;
        typedef Json::Value& ObjectRef;
        typedef const Json::Value& ObjectConstRef;

        typedef Json::ValueIterator ObjectIterator;
        typedef Json::ValueConstIterator ObjectConstIterator;

        static Object emptyObject(Json::objectValue);

        class ObjectFormatError :  public std::exception
        {
        public:
            ObjectFormatError(const std::string& aDetail,
                              const std::string& aSource)
                : mMsg(aDetail),mSource(aSource) {}
            ~ObjectFormatError() {}
            const char* what() const throw() { return mMsg.c_str(); }
            const std::string& source() { return mSource; }
        protected:
            std::string mMsg;
            std::string mSource;
        };

        inline void stringToObject(const std::string& s,ObjectRef o) {
            Json::Reader reader;
            if (!reader.parse(s, o))
                throw ObjectFormatError(reader.getFormattedErrorMessages(),s);
        }
        // non-human-readable, compact with minimal formatting
        inline std::string objectToString(ObjectConstRef o) {
            Json::FastWriter writer;
            return writer.write(o);
        }
        // human-readable formatting
        inline std::string objectToStyledString(ObjectConstRef o) {
            Json::StyledWriter writer;
            return writer.write(o);
        }
        
        // uses no formatting at all : intended mainly for single values.
        // Top-level strings are only escaped and enclosed in quotes if 
        // 'quoteStrings' is true, although strings inside objects/arrays 
        // are always quoted.
        std::string valueToString(ObjectConstRef o,bool quoteStrings);
}
}
#endif
