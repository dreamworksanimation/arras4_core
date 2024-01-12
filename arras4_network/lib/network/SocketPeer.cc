// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "platform.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include <poll.h>
#include <chrono>

#include "SocketPeer.h"
#include "Encryption.h"
#include "InvalidParameterError.h"

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

#include <linux/un.h>

namespace {

// system calls can return an EINTR error which just says that
// an interrupt happened before it got around to doing it's thing.
// Rather than scattering code around to deal with this inconvenient
// behavior these functions just wrap the I/O functions and watch for
// that case and simply retries. 

int
close_ignore_interrupts(int fd)
{
    int status = 0;

    do {
        status = close(fd);
    } while ((status < 0) && (errno == EINTR));

    return status;
}

ssize_t
recv_ignore_interrupts(int sockfd, void* buf, size_t len, int flags)
{
    ssize_t status = 0;

    do {
        status = ::recv(sockfd, buf, len, flags);
    } while ((status < 0) && (errno == EINTR));
    
    return status;
}

ssize_t
send_ignore_interrupts(int sockfd, const void* buf, size_t len, int flags)
{
    ssize_t status = 0;

    do {
        status = ::send(sockfd, buf, len, flags);
    } while ((status < 0) && (errno == EINTR));

    return status;
}

// this version of poll won't handle the timeout properly since the
// call will be remade with the entire timeout properly. 0 (return immediately)
// and -1 (no timeout) will work the same though.
int
poll_ignore_interrupts(struct pollfd* fds, nfds_t nfds, int timeout)
{
    int status = 0;

    do {
        status = ::poll(fds, nfds, timeout);
    } while ((status < 0) && (errno == EINTR));

    return status;
}

int
accept4_ignore_interrupts(int sockfd, struct sockaddr* addr, socklen_t* addrlen, int flags)
{
    int status = 0;

    do {
        status = ::accept4(sockfd, addr, addrlen, flags);
    } while ((status < 0) && (errno == EINTR));

    return status;
}

}

namespace arras4 {
    namespace network {

// strerror isn't thread safe because it uses an internal buffer for
// returning the string so we need to use strerror_r which takes a
// user provided buffer. Rather than worry about allocating and
// freeing a char array just stuff it into a std::string.
std::string
SocketPeer::getErrorString(int aErrno)
{
    char buffer[200];
    buffer[0] = 0;
    return std::string(strerror_r(aErrno, buffer, sizeof(buffer)));
}

int
SocketPeer::getSocketError()
{
    return errno;
}

int
SocketPeer::getLookupError()
{
    return h_errno;
}

PeerException::Code
SocketPeer::getCodeFromSocketError(int aErrno)
{
    switch (aErrno) {
    case EACCES: return PeerException::PERMISSION_DENIED;
    case EAFNOSUPPORT: return PeerException::UNSUPPORTED_ADDRESS_FAMILY;
    case EINVAL: return PeerException::INVALID_OPERATION;
    case EMFILE: return PeerException::FILES;
    case EISDIR: return PeerException::FILES;
    case ENOTDIR: return PeerException::FILES;
    case ENOENT: return PeerException::FILES;
    case ENFILE: return PeerException::FILES;
    case ENOBUFS: return PeerException::INSUFFICIENT_MEMORY;
    case ENOMEM: return PeerException::INSUFFICIENT_MEMORY;
    case EPROTONOSUPPORT: return PeerException::INVALID_PROTOCOL;
    case EPROTO: return PeerException::INVALID_PROTOCOL;
    case EBADF: return PeerException::INVALID_DESCRIPTOR;
    case ECONNREFUSED: return PeerException::CONNECTION_REFUSED;
    case ECONNABORTED: return PeerException::CONNECTION_ABORT;
    case ECONNRESET: return PeerException::CONNECTION_RESET;
    case EPIPE: return PeerException::CONNECTION_CLOSED;
    case EFAULT: return PeerException::INVALID_PARAMETER;
    case EINTR: return PeerException::INTERRUPTED;
    case ENOTCONN: return PeerException::NOT_CONNECTED;
    case ENOTSOCK: return PeerException::INVALID_DESCRIPTOR;
    case EADDRINUSE: return PeerException::IN_USE;
    case EOPNOTSUPP: return PeerException::INVALID_OPERATION;
    case ETIMEDOUT: return PeerException::TIMEOUT;
    default: return PeerException::UNKNOWN;
    }
}

PeerException::Code
SocketPeer::getCodeFromLookupError(int aErrno)
{
    switch (aErrno) {
    case HOST_NOT_FOUND: return PeerException::NO_HOST;
    case NO_ADDRESS: return PeerException::ADDRESS_NOT_FOUND;
    case NO_RECOVERY: return PeerException::NAME_SERVER_ERROR;
    case TRY_AGAIN: return PeerException::NAME_SERVER_ERROR;
    default: return PeerException::UNKNOWN;
    }
}

PeerException::Code
SocketPeer::getCodeFromGetAddrInfoError(int aErrno)
{
    switch (aErrno) {
    case EAI_FAIL:
    case EAI_NODATA:
    case EAI_NONAME:
        return PeerException::NO_HOST;
    case EAI_ADDRFAMILY:
    case EAI_BADFLAGS:
    case EAI_FAMILY:
    case EAI_SYSTEM:
    case EAI_MEMORY:
    case EAI_SERVICE:
    case EAI_SOCKTYPE:
    default:
        return PeerException::UNKNOWN;
  
    }
}


SocketPeer::SocketPeer()
    : mSocket(ARRAS_INVALID_SOCKET)
    , mIsListening(false)
    , mBytesRead(0)
    , mBytesWritten(0)
{
}

SocketPeer::SocketPeer(ARRAS_SOCKET sock)
    : mSocket(ARRAS_INVALID_SOCKET)
    , mIsListening(false)
    , mBytesRead(0)
    , mBytesWritten(0)
{
    struct stat stat_info;
    if (fstat(sock, &stat_info) < 0) {
        int save_errno = errno;

        if (save_errno == EBADF) {
            throw InvalidParameterError("Bad file descriptor");
        } else {
            throw InvalidParameterError("Problem calling fstat on file descriptor");
        }
    }

    if (!S_ISSOCK(stat_info.st_mode)) {
        throw InvalidParameterError("File descriptor is not a socket");
    }

    // find out if it is a listening socket
    int isListening = false;

    socklen_t size = sizeof(isListening);
    if (getsockopt(sock, SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<char*>(&isListening), &size)) {
        // save errno before doing anything else
        int save_errno = errno;

        std::string err("SocketPeer: Problem calling getsockopt on the socket ");
        err += getErrorString(save_errno);
        
        // this shouldn't ever get an error but check just in case
        throw InvalidParameterError(err.c_str());
    } else {
        mIsListening = isListening;
    }

    mSocket = sock;
}
        
SocketPeer::~SocketPeer()
{
    shutdown();
}

void
SocketPeer::shutdown()
{
    if (mSocket == ARRAS_INVALID_SOCKET) return;

    if (mEncryption != nullptr) { 
        mEncryption->shutdown_send();
    }

    ::shutdown(mSocket, SHUT_RDWR);
    close_ignore_interrupts(mSocket);

    mSocket = ARRAS_INVALID_SOCKET;
    mIsListening = false;
}

void
SocketPeer::shutdown_send()
{
    if (mSocket == ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("Attempted shutdown_send on an uninitialized peer");
    }
    if (mEncryption != nullptr) { 
        mEncryption->shutdown_send();
    }
    ::shutdown(mSocket, SHUT_WR);
}

void
SocketPeer::shutdown_receive()
{
    if (mSocket == ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("Attempted shutdown_send on an uninitialized peer");
    }
    ::shutdown(mSocket, SHUT_RD);
}

bool
SocketPeer::send(const void *data, size_t nBytes)
{
    // early out if there is nothing to write
    // don't care whether pointer is valid if count is 0
    if (nBytes == 0) {
        return true;
    }

    if (data == nullptr) {
        throw InvalidParameterError("SocketPeer::send invalid null data ptr");
    }
    if (mIsListening) {
        throw InvalidParameterError("SocketPeer::send on an listening socket");
    }

    int flags = MSG_NOSIGNAL;

    // if there is an encryption object then delegate the write
    if (mEncryption != nullptr) {
        bool rtn = 0;

        try {
            rtn = mEncryption->write(data, nBytes);
        } catch (const EncryptException& e) {
            throw PeerException(e.what());
        }
        mBytesWritten += nBytes;
        return rtn;
    }


    // need to cast the void* to a uint8* so pointer math can be done 
    uint8_t* data8 = static_cast<uint8_t*>(const_cast<void*>(data));
    size_t offset = 0;

    // Keep sending until all the data is sent or there is an error
    while (nBytes > 0) {

        ssize_t status = send_ignore_interrupts(mSocket, (const char*)data8 + offset, nBytes, flags);
        if (status < 0) {
            // save errno before doing anything else
            int save_errno = getSocketError();

            // throw an exception
            std::string err("SocketPeer::send: ");
            err += getErrorString(save_errno);
            throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
        }

        // if the remote endpoint has stopped accepting data
        // before anything has been sent then return false
        // if a partial message is sent then throw an exception
        if (status == 0) {
            if (offset == 0) {
                return false;
            } else {
                throw_disconnect("SocketPeer::send partial message sent");
            }
        }

        offset += status;
        nBytes -= status;
    }
    mBytesWritten += nBytes;
    return true;
}

size_t
SocketPeer::receive(void *buffer, size_t nMaxBytesToRead)
{
    // returning zero when zero bytes are requested wouldn't be distinquishable from
    // a disconnect so just consider a read of zero bytes an invalid parameter
    if (nMaxBytesToRead == 0) {
        throw InvalidParameterError("SocketPeer::receive invalid byte count of 0");
    }
    if (mIsListening) {
        throw InvalidParameterError("SocketPeer::receive on an listening socket");
    }

    if (buffer == nullptr) {
        throw InvalidParameterError("SocketPeer::receive invalid null data ptr");
    }

    // if there is an encryption object then delegate the read
    if (mEncryption) {
        ssize_t rtn;

        try {
            rtn = mEncryption->read(buffer, nMaxBytesToRead);
        } catch (const EncryptException& e) {
            throw PeerException(e.what());
        }
        if (rtn < 0) {
            throw PeerException("SocketPeer::receive error doing encrypted read");
        }
        mBytesRead += rtn;
        return rtn;
    }

    int flags = MSG_NOSIGNAL;

    // do a single recv(). It won't necessarily get all of the data
    ssize_t rtn = recv_ignore_interrupts(mSocket, (char*)buffer, nMaxBytesToRead, flags);
    if (rtn < 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

	// throw an exception
        std::string err("SocketPeer::receive: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }
    mBytesRead += rtn;
    // let a zero be returned
    return rtn;
}

bool
SocketPeer::receive_all(void* buffer, size_t nBytesToRead, unsigned int aTimeoutMs)
{
    // need to cast the void* to a uint8* so pointer math can be done
    uint8_t* data8 = static_cast<uint8_t*>(buffer);
    size_t offset = 0;
    size_t nBytes = nBytesToRead;
    struct timeval remaining;
    struct timeval endtime;

    if (aTimeoutMs > 0) {
        remaining.tv_sec = aTimeoutMs / 1000;
        remaining.tv_usec = (aTimeoutMs % 1000) * 1000;
        gettimeofday(&endtime, nullptr);
        timeradd(&endtime, &remaining, &endtime);
    }

    // Keep receiving until all the data is received or there is an error or timeout
    size_t status;
    while (nBytes > 0) {

        if (aTimeoutMs > 0) {
            struct timeval now;
            gettimeofday(&now, nullptr);
            // if the time expired between the last poll and getting back to here do one last check
            if (timercmp(&now, &endtime, > )) {
                remaining.tv_sec = 0;
                remaining.tv_usec = 0;
            } else {
                timersub(&endtime, &now, &remaining);
            }
            unsigned int remainingTimeoutMs = (unsigned int)(remaining.tv_sec * 1000 + (remaining.tv_usec + 999) / 1000);

            bool read_status;
            bool write_status;
            // don't care about return value because receive() will take care of disconnects
            poll(true, read_status, false, write_status, remainingTimeoutMs);
            if (!read_status) {
                throw PeerException(ETIME, PeerException::TIMEOUT, "SocketPeer::receive_all: Timeout expire");
            }
        }

        status = 0;
        try {
            status = receive(data8 + offset, nBytes);
        } catch (PeerDisconnectException&) {
            // just let the status stay at zero and respond just
            // like it returned 0
        }

        // if it got PeerDisconnectException or returned 0 how it
        // responds depends on whether it has read any data
        // Failure to read any data at all would be someting that
        // occurs in normal operations when the shutdown isn't negotiated
        // in application code. A partial message is an clear indication
        // of an unexpected shutdown
        if (status == 0) {
            if (nBytes == nBytesToRead) {
                // return false if it simply couldn't read anything
                return false;
            } else {
                // a partial message is an exception
                // should there be a separate exception for this?
                throw_disconnect("SocketPeer::receive_all partial receive");
            }
        }

        offset += status;
        nBytes -= status;
    }

    return true;
}

// shutdown the connection in a way which will prevent other threads from
// blocking on reads and writes to the socket but which won't create
// race conditions. A full shutdown of the socket would free up the file id
// for reuse in a race condition. By only doing a ::shutdown() reads and
// writes will return immediately with an error but the file id is still
// kept out of circulation
//
void
SocketPeer::threadSafeShutdown()
{
    if (mSocket >= 0) {
        ::shutdown(mSocket, SHUT_RDWR);
    }
}

size_t
SocketPeer::peek(void *buffer, size_t nMaxBytesToRead)
{
    // returning zero when zero bytes are requested wouldn't be distinquishable from
    // a disconnect so just consider a read of zero bytes an invalid parameter
    if (nMaxBytesToRead == 0) {
        throw InvalidParameterError("SocketPeer::peek invalid byte count of 0");
    }
    if (mIsListening) {
        throw InvalidParameterError("SocketPeer::peek on an listening socket");
    }

    if (buffer == nullptr) {
        throw InvalidParameterError("SocketPeer::peek invalid null data ptr");
    }

    // if there is an encryption object then delegate the peek
    if (mEncryption) {
        ssize_t rtn = 0;

        try {
            rtn = mEncryption->peek(buffer, nMaxBytesToRead);
        } catch (const EncryptException& e) {
            throw PeerException(e.what());
        }

        if (rtn < 0) {
            throw PeerException("SocketPeer::peek error doing encrypted read");
        }

        return rtn;
    }

    int flags = MSG_NOSIGNAL;

    ssize_t rtn = recv_ignore_interrupts(mSocket, (char*)buffer, nMaxBytesToRead, flags|MSG_PEEK);
    if (rtn < 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // throw an exception
        std::string err("SocketPeer::peek: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err.c_str());
    }

    return rtn;
}

// if query_read is true, poll for read events and set read if one occurs within the timeout
// if query_write is true, poll for write events and set write if one occurs within the timeout
// Returns true if the socket has been closed. Note that shutdown counts as a read event, and
// so will true only be returned if query_read is true : in this case read will also be set to true.
// i.e. ret=false, read=true => still open, data ready to read
//      ret=true,  read=true => socket closed
bool
SocketPeer::poll(
    bool query_read, bool& read,
    bool query_write, bool& write,
    unsigned int timeoutMs)
{
    // clear them
    if (query_read) read = false;
    if (query_write) write = false;

    // if socket is not initialized. This should only happen if there is a programming error.
    if (mSocket == ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("Attempted poll on a uninitialized peer");
    }

    // if we are listening, then we are a server and any activity on
    // our socket should be treated as an incoming connection
    if (mIsListening) {
        throw InvalidParameterError("Attempted polling socket which is listening");
    }

    if (!query_read && !query_write) {
        throw InvalidParameterError("Neither read nor status is being queried");
    }

    // create a pollfd struct to monitor a single file descriptor
    struct pollfd pfd;
    pfd.fd = mSocket;
    pfd.events = 0;
    if (query_read)
        pfd.events |= POLLIN;
    if (query_write)
        pfd.events |= POLLOUT;

    // openssl may have read all of the available data such that
    // there will be nothing on the socket to read but a SocketPeer::receive
    // will return data due to openssl buffering.
    // if pending() > 0 then there is something to be read regardless of what
    // select has to say about it.
    if (query_read && (mEncryption != nullptr)) {
        try {
            if (mEncryption->pending() > 0) {
                read = true;
                query_read = false;

                // if only reads were queried than go ahead and return
                // since there is nothing else to check
                if (!query_write) return false;

                // we know there is at least immediate read data so clear the timeout
                // so we won't pause waiting for write capability
                timeoutMs = 0;

            }
        } catch (const EncryptException& e) {
            throw PeerException(e.what());
        }
    }
            
    int r = 0; // return value of poll()   

    if (timeoutMs == 0) {

        // timeout of 0 causes the function to return immediately
        r = poll_ignore_interrupts(&pfd, 1, 0);
        
        // if r==0 we didn't find anything
        if (r==0) return false;

    } else {

        // poll can return before the timeout when there is a signal
        // so to honor the timeout we need to loop on the call to select
        // using a newly computed timeout each time

        // time point that timeout expires
        std::chrono::steady_clock::time_point endTime = 
            std::chrono::steady_clock::now() + 
            std::chrono::milliseconds(timeoutMs);

        // remaining milliseconds until timeout expires
        int remaining = (int)timeoutMs; 

        while (true) {
 
            // don't use poll_ignore_interrupts() because we have a timeout to deal with
            r = ::poll(&pfd, 1, remaining);
           
            // if r == 0 we didn't find anything
            if (r==0) {
                return false;
            }
            // check for error EINTR ( = "a signal occured before timeout elapsed")
            else if ((r == -1) && (errno == EINTR)) {

                // calculate how many milliseconds of timeout remain
                std::chrono::steady_clock::duration left =  endTime - std::chrono::steady_clock::now();
                std::chrono::milliseconds msLeft = std::chrono::duration_cast<std::chrono::milliseconds> (left);
                remaining = (int)msLeft.count();
                // if remaining <= 0 we didn't find anything before the timeout, so
                // leave read & write at their previously set false values and exit
                if (remaining <= 0)
                    return false;
            }
            else {
                // success (r=1) or some other error (r=-1)
                break;
            }
        }
    }


    // now r is the return value from poll(), either -1 (for error), or 1 (for success)
    if (r == -1) {

        // indicates an error
        int save_errno = getSocketError();

        // throw an exception
        shutdown();
        std::string err("SocketPeer::poll: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    // looks like we found an event...

    bool readSet = (pfd.revents & POLLIN) != 0;
    bool writeSet = (pfd.revents & POLLOUT) != 0;
    bool returnValue = false;

    if (query_read && readSet) {
        
        read = true;

        // read event might be that data is ready to read, or might be a shutdown.
        // distinguish between the two by trying to peek a byte
        unsigned char p[1];      
        ssize_t n = recv_ignore_interrupts(mSocket, (char*)p, 1, MSG_PEEK);
  
        if (n < 0) {
            // error peeking socket
            // save errno before doing anything else
            int save_errno = errno;

            // clean up
            shutdown();

            // throw an exception
            std::string err("SocketPeer::poll: ");
            err += getErrorString(save_errno);
            throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
        }

        if (n == 0) {
            // the remote endpoint has closed so return true to let the caller know
            // that the connection is closing down
            returnValue = true;
        }
    }

    if (query_write && writeSet) {
        write = true;
    }

    return returnValue;
}

void
SocketPeer::accept(Peer**& peers, int& nPeers, unsigned int timeoutMs)
{
    // if socket is not initialized. This should only happen if there is a programming error.
    if (mSocket == ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("Attempted accept on a uninitialized peer");
    }

    // if we are not listening, then we are not a server and any activity on
    // our socket should be treated as incoming data
    if (!mIsListening) {
        throw InvalidParameterError("Attempted accept on a socket which isn't listening");
    }

    // create a pollfd struct to monitor a single file descriptor
    struct pollfd pfd;
    pfd.fd = mSocket;
    pfd.events = POLLIN; // we ignore write FDs since we use blocking writes on client sockets
    
    int r = 0; // return value of poll()
  
    if (timeoutMs == 0) {

        // timeout of 0 causes the function to return immediately
        r = poll_ignore_interrupts(&pfd, 1, 0);

    } else {

        // poll can return before the timeout when there is a signal
        // so to honor the timeout we need to loop on the call to select
        // using a newly computed timeout each time

        // time point that timeout expires
        std::chrono::steady_clock::time_point endTime = 
            std::chrono::steady_clock::now() + 
            std::chrono::milliseconds(timeoutMs);

        // remaining millis until timeout expires
        int remaining = (int)timeoutMs; 

        while (true) {

            // don't use poll_ignore_interrupts() because we have a timeout to deal with
            r = ::poll(&pfd, 1, remaining);

            // check for error EINTR ( = "a signal occured before timeout elapsed")
            if ((r == -1) && (errno == EINTR)) {

                // calculate how many millis of timeout remain
                std::chrono::steady_clock::duration left =  endTime - std::chrono::steady_clock::now();
                std::chrono::milliseconds msLeft = std::chrono::duration_cast<std::chrono::milliseconds> (left);
                remaining = (int)msLeft.count();
                // if time has run out, set r = 0 to indicate no events were detected
                if (remaining <= 0) {
                    r = 0;
                    break;
                }
                // otherwise, call poll() again for the remaining time
            }
            else {
                break;
            }
        } 
    }

    // check poll return value r : -1 => error
    if (r == -1) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // throw an exception
        shutdown();
        std::string err("SocketPeer::accept: ");
        err += getErrorString(save_errno);
        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err.c_str());
    }

    // return code of zero means timeout, we can ignore that (means nothing
    // worthwhile happened during that time)
    if (r == 0) {
        nPeers = 0;
        return;
    }
 
    // since we only polled one file descriptor and r >= 1,
    // the socket must be ready for read
    acceptAll(peers, nPeers);
}

void
SocketPeer::acceptAll(Peer**& peers, int& nPeers)
{
    int maxPeers = nPeers;
    nPeers = 0;
    ARRAS_SOCKET newSocket = ARRAS_INVALID_SOCKET;

    // if socket is not initialized. This should only happen if there is a programming error.
    if (mSocket == ARRAS_INVALID_SOCKET) {
        throw InvalidParameterError("Attempted accept on a uninitialized peer");
    }

    // if we are not listening, then we are not a server and any activity on
    // our socket should be treated as incoming data
    if (!mIsListening) {
        throw InvalidParameterError("Attempted accept on a socket which isn't listening");
    }

    do {
        newSocket = accept4_ignore_interrupts(mSocket, nullptr, nullptr, ARRAS_SOCK_CLOEXEC);

        // there was an error
        if (newSocket == ARRAS_INVALID_SOCKET) {
            // save errno before doing anything else
            int save_errno = getSocketError();

            // EWOULDBLOCK would indicate that there is
            // nothing to accept though if everyone is using this
            // correctly we should already know that there are connections
            // to be accepted.
            if ((save_errno != EWOULDBLOCK) && (save_errno != EAGAIN)) {

                // throw an exception
                std::string err("SocketPeer::acceptAll: ");
                err += getErrorString(save_errno);
                throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
            }
            return;
        }

        // a non-negative value is a file descriptor for a socket

        // we want blocking IO on the client sockets (for write)
        int no = 0;
        if (ioctl(newSocket, FIONBIO, (char*)&no) < 0) {
            close_ignore_interrupts(newSocket);
            continue;
        }

        // create new Peer for this connection
        SocketPeer* peer = new SocketPeer(newSocket);
        peers[nPeers++] = peer;
    } while (nPeers < maxPeers);
}

void
SocketPeer::setEncryption(std::unique_ptr<EncryptState> aEncryption)
{
    mEncryption = std::move(aEncryption);
}

ARRAS_SOCKET
SocketPeer::fd()
{
    return mSocket;
}

void
SocketPeer::enableKeepAlive()
{
    const int enabled = 1;
    if (setsockopt(mSocket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&enabled, sizeof(enabled)) < 0) {
        // save errno before doing anything else
        int save_errno = getSocketError();

        // clean up
        shutdown();

        // throw an exception
        std::string err("Couldn't enable keepalive: ");
        err += getErrorString(save_errno);

        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
    }

    int keepTime = mKeepAliveSettings.keepTime;
    int keepInvl = mKeepAliveSettings.keepInvl;
    int keepProbe = mKeepAliveSettings.keepProbe;

    setsockopt(mSocket, IPPROTO_TCP, TCP_KEEPIDLE, &keepTime, sizeof(keepTime));
    setsockopt(mSocket, IPPROTO_TCP, TCP_KEEPINTVL, &keepInvl, sizeof(keepInvl));
    setsockopt(mSocket, IPPROTO_TCP, TCP_KEEPCNT, &keepProbe, sizeof(keepProbe));
}

} // namespace network
} // namespace arras

