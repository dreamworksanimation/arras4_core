// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CLIENT_EXCEPTION_H__
#define __ARRAS4_CLIENT_EXCEPTION_H__

#include <exception>
#include <stddef.h>
#include <string>

namespace arras4 {
    namespace client {

class ClientException : public std::exception {
public:
    enum Type {
        UNKNOWN_ERROR = -1,           //!  An unknown, unexpected error
        GENERAL_ERROR = 0,            //!  A general error that doesn't match other specific conditions
        CONNECTION_ERROR,             //!  An error relating to connecting to the service
        SERVICE_REQUEST_ERROR,        //!  An error indicating the requested session does not exist
        SEND_ERROR,                   //!  An error sending a message to the service
        AUTHENTICATION_ERROR,         //!  Authentication related error.
        NO_AVAILABLE_RESOURCES_ERROR //!  An error indicating no resources are available for the session
    };
    ClientException(const char* msg) : std::exception(), mWhat(msg), mType(UNKNOWN_ERROR) {}
    ClientException(const std::string& msg) : std::exception(), mWhat(msg), mType(UNKNOWN_ERROR) {}
    ClientException(const char* msg, Type type) : std::exception(), mWhat(msg), mType(type) {}
    ClientException(const std::string& msg, Type type) : std::exception(), mWhat(msg), mType(type) {}

    ~ClientException() throw() {}

    const char* what() const throw() { return mWhat.c_str(); }
    Type getType() const { return mType; }

protected:
    std::string mWhat;
    Type mType;

};

}
}
#endif
