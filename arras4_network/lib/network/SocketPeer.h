// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_SOCKETPEER_H__
#define __ARRAS4_SOCKETPEER_H__

#include "Peer.h"
#include "PeerException.h"
#include "platform.h"
#include <memory>

namespace arras4 {
    namespace network {

class EncryptState;

class SocketPeer : public Peer
{
protected:
    // this ctor will create an "owned" socket
    // there is no reason for a user to create a default constructed
    // SocketPeer since it isn't possible to connect it with
    // anything. This is only used by derived classes
    SocketPeer();
    void shutdown_internal();
    static std::string getErrorString(int aErrno);
    int getSocketError();
    static PeerException::Code getCodeFromSocketError(int aErrno);
    int getLookupError();
    static PeerException::Code getCodeFromLookupError(int aErrno);
    static PeerException::Code getCodeFromGetAddrInfoError(int aErrno);

public:
    // this ctor will simply "wrap" an existing socket
    // when the socket already exists it can just be a generic SocketPeer
    // rather than one of the derived classes
    SocketPeer(ARRAS_SOCKET sock);
    ~SocketPeer();

    // Peer implementation
    void shutdown();

    // shutdown the connection in a way which will prevent other threads from
    // blocking on reads and writes to the socket but which won't create
    // race conditions. A full shutdown of the socket would free up the file id
    // for reuse in a race condition. By only doing a ::shutdown() reads and
    // writes will return immediately with an error but the file id is still
    // kept out of circulation
    void threadSafeShutdown();

    void shutdown_send(); // this connection won't be sending me data
    void shutdown_receive(); // this connection will not accept more data
    bool send(const void* data, size_t nBytes);
    size_t receive(void* buffer, size_t nMaxBytesToRead);
    bool receive_all(void* buffer, size_t nBytesToRead, unsigned int aTimeoutMs = 0);
    size_t peek(void* buffer, size_t nMaxBytesToRead);
    bool poll(bool query_read, bool& read,
              bool query_write, bool& write,
              unsigned int timeoutMs = 0);

    // accept any pending incoming connections; uses required SecurityProvider to allow or deny potential
    // new peer(s); returns list of pointers to any new allowed Peers in "peers" and number of pointers
    // in "nPeers" (supply length of "peers" in nPeers when calling)
    void accept(Peer**& peers, int& nPeers, unsigned int timeoutMs = 0);

    // HACK -- the select was done externally, and we know there are connections to accept...
    void acceptAll(Peer**& peers, int& nPeers);

    // layer encryption over the socket connection
    void setEncryption(std::unique_ptr<EncryptState> aState);

    ARRAS_SOCKET fd();

    size_t bytesRead() const { return mBytesRead; }
    size_t bytesWritten() const { return mBytesWritten; }

protected:
    // Settings for keep alive
    struct KeepAliveSettings {
        KeepAliveSettings() : keepTime(300), keepInvl(75), keepProbe(9) {}
        
        int keepTime;
        int keepInvl;
        int keepProbe;

    } mKeepAliveSettings;

    void enableKeepAlive();

    ARRAS_SOCKET mSocket;

    // when an encryption module is attached, this will be non-null
    std::unique_ptr<EncryptState> mEncryption;

    bool mIsListening;
    size_t mBytesRead;
    size_t mBytesWritten;
};

} // namespace network
} // namespace arras4

#endif // __ARRAS4_SOCKETPEER_H__

