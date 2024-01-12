// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_PEER_H__
#define __ARRAS4_PEER_H__

#include "PeerException.h"
#include "DataSource.h"
#include "DataSink.h"
#include <string>
#include <chrono>

namespace arras4 {
    namespace network {

class Peer;

class PeerSourceAndSink : public DataSource, public DataSink {
public:
    PeerSourceAndSink(Peer& peer) 
        : mPeer(peer)
        {}
    size_t read(unsigned char* aBuf, size_t aLen); 
    size_t skip(size_t aLen);
    size_t bytesRead() const;  
    size_t write(const unsigned char* aBuf, size_t aLen);
    void flush();
    size_t bytesWritten() const;

private:
    Peer& mPeer;
};

class Peer {
public:
    Peer() : mSourceAndSink(*this) {}
    virtual ~Peer() {}

    // orderly shutdown of this endpoint (blocking until shutdown complete)
    virtual void shutdown() = 0;
    virtual void shutdown_send() = 0;
    virtual void shutdown_receive() = 0;

    // shutdown the connection in a way that is safe for the case of other
    // threads actively reading or writing it.
    virtual void threadSafeShutdown() = 0;

    // send data to the remote endpoint (blocking); returns number of bytes sent
    virtual bool send(const void* data, size_t nBytes) = 0;
    void send_or_throw(const void* data, size_t nBytes, const char* message) {
        if (!send(data, nBytes)) {
            throw_disconnect(message);
        }
    }

    // receive data from the remote endpoint (blocking); returns number of bytes read
    // by 'src' will contain the IPv4/IPv6 address/port source system information
    virtual size_t receive(void* buffer, size_t nMaxBytesToRead) = 0;

    // receive all the data and return true, receive no data and return false, or throw an exception
    virtual bool receive_all(void* buffer, size_t nBytesToRead, unsigned int aTimeoutMs = 0) = 0;
    // convenience function for requiring success
    void receive_all_or_throw(void* buffer, size_t nBytesToRead,
                              const char* message,  unsigned int aTimeoutMs = 0) {
        if (!receive_all(buffer, nBytesToRead, aTimeoutMs)) {
            throw_disconnect(message);
        }
    }

    // "peek" at available data to read, without consuming it from the incoming stream
    virtual size_t peek(void* buffer, size_t nMaxBytesToPeek) = 0;
    size_t peek_or_throw(void* buffer, size_t nMaxBytesToPeek, const char* message) {
        size_t status = peek(buffer, nMaxBytesToPeek);
        if (status == 0) {
            throw_disconnect(message);
        }
        return status;
    }

    // Check to see if any data is available to read (non-blocking); on return, if data is available to
    // read, "read" will be true; if transport is available for writing, "write" will be true
    virtual bool poll(/*in*/ const bool query_read, /*out*/ bool& read,
                      /*in*/ const bool query_write, /*out*/bool& write,
                      /*in*/ unsigned int timeoutMs = 0) = 0;

    void throw_disconnect(const char* aMsg) {
        std::string message(aMsg);
        message += " - Remote endpoint closed connection";
        throw PeerDisconnectException(aMsg);
    }

    virtual size_t bytesRead() const =0;
    virtual size_t bytesWritten() const =0;

    network::DataSource& source() { return mSourceAndSink; }
    network::DataSink& sink() { return mSourceAndSink; }

private:
    PeerSourceAndSink mSourceAndSink;

};

} // namespace network
} // namespace arras

#endif // __ARRAS_PEER_H__

