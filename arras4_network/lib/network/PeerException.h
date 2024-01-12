// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PEER_EXCEPTION_H__
#define __ARRAS4_PEER_EXCEPTION_H__

#include <exception>
#include <string>

namespace arras4 {
    namespace network {

class PeerException : public std::exception
{
public:
    enum Code {
        IN_USE,
        NO_HOST,
        ADDRESS_NOT_FOUND,
        NAME_SERVER_ERROR,
        CONNECTION_CLOSED,
        CONNECTION_REFUSED,
        CONNECTION_RESET,
        CONNECTION_ABORT,
        NOT_CONNECTED,
        PERMISSION_DENIED,
        UNSUPPORTED_ADDRESS_FAMILY,
        INVALID_OPERATION,
        INVALID_PARAMETER,
        INVALID_PROTOCOL,
        INVALID_DESCRIPTOR,
        FILES,
        INSUFFICIENT_MEMORY,
        INTERRUPTED,
        TIMEOUT,
        NOT_INITIALIZED,
        UNKNOWN,
    };

    PeerException(int aErrno, Code aCode, const std::string& aMsg)
        : mMsg(aMsg), mErrno(aErrno), mCode(aCode)  {}
    PeerException(const std::string& aMsg)
        : PeerException(0, UNKNOWN, aMsg) {}
    PeerException(int aErrno)
        : PeerException(aErrno, UNKNOWN, "") {}
    PeerException(int aErrno, Code aCode)
        : PeerException(aErrno, aCode, "") {}
   
    PeerException(int aErrno, const std::string& aMsg)
        : PeerException(aErrno, UNKNOWN, aMsg) {}
    PeerException(Code aCode, const std::string& aMsg )
        : PeerException(0, aCode, aMsg) {}

    ~PeerException() throw() {}
    const char* what() const throw() { return mMsg.c_str(); }
    int error() const { return mErrno; }
    Code code() const { return mCode; }

protected:
    std::string mMsg;
    int mErrno;
    Code mCode;
};

class PeerDisconnectException : public PeerException
{
public:
    PeerDisconnectException(const std::string& aMsg)
        : PeerException(CONNECTION_CLOSED, aMsg) {}
};

}
}
#endif
