// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_ENCRYPTION_H__
#define __ARRAS4_ENCRYPTION_H__

#include "platform.h"
#include <exception>
#include <memory>
#include <string>

#ifdef _MSC_VER
#define ssize_t SSIZE_T
#include <basetsd.h>
#endif

/**
 *  Encryption base classes
 */

namespace arras4 {
    namespace network {

        class SocketPeer;

        class EncryptException : public std::exception
        {
        public:
            EncryptException(const char* aMsg);
            ~EncryptException() throw();
            const char* what() const throw();

        protected:
            std::string mMsg;
        };

        class EncryptContext
        {
        public:
            // required virtual destructor
            virtual ~EncryptContext();

            // attach the encryption module to the SocketPeer; will perform TLS negotiation
            // and all further traffic on the peer will be encrypted
            virtual void encryptConnection(SocketPeer& aPeer) = 0;
        };

        class EncryptState
        {
        public:
            virtual ~EncryptState();
            virtual ssize_t read(void* aBuf, size_t aMaxLen) = 0;
            virtual ssize_t peek(void* aBuf, size_t aMaxLen) = 0;
            virtual bool write(const void* aBuf, size_t aLen) = 0;
            virtual size_t pending() = 0;
            virtual void shutdown_send() = 0;
        };

    } // namespace network
} // namespace arras4

#endif // __ARRAS_ENCRYPTION_H__

