// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "network_platform.h"

#ifdef PLATFORM_WINDOWS

#include <winsock2.h>
#include <BaseTsd.h>
#include <WS2tcpip.h>
#include "mstcpip.h"
typedef SSIZE_T ssize_t;

#include "windows_utils.h"

#else

    #ifdef PLATFORM_APPLE
        #include <libproc.h>
        #include <netinet/tcp_fsm.h>
    #endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#define USE_POLL 1
#include <poll.h>

#endif // PLATFORM_WINDOWS

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

#if !defined(timeradd)
#define timeradd(tvp, uvp, vvp)                                         \
        do {                                                            \
                (vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;          \
                (vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;       \
                if ((vvp)->tv_usec >= 1000000) {                        \
                        (vvp)->tv_sec++;                                \
                        (vvp)->tv_usec -= 1000000;                      \
                }                                                       \
        } while (0)
#endif
#if !defined(timersub)
#define timersub(tvp, uvp, vvp)                                         \
        do {                                                            \
                (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;          \
                (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;       \
                if ((vvp)->tv_usec < 0) {                               \
                        (vvp)->tv_sec--;                                \
                        (vvp)->tv_usec += 1000000;                      \
                }                                                       \
        } while (0)
#endif

#ifdef PLATFORM_LINUX
#include <linux/un.h>
#endif

namespace {

    int get_socket_error()
    {
#ifdef PLATFORM_WINDOWS
        return WSAGetLastError();
#else
        return errno;
#endif
    }


    // system calls can return an EINTR error which just says that
    // an interrupt happened before it got around to doing it's thing.
    // Rather than scattering code around to deal with this inconvenient
    // behavior these functions just wrap the I/O functions and watch for
    // that case and simply retries. 
    int
        close_ignore_interrupts(ARRAS_SOCKET s)
    {
        int status = 0;

        do {
#ifdef PLATFORM_WINDOWS
            status = closesocket(s);
#else
            status = close(s);
#endif
        } while ((status < 0) && (get_socket_error() == EINTR));

        return status;
    }

    ssize_t
        recv_ignore_interrupts(ARRAS_SOCKET s, char* buf, size_t len, int flags)
    {
        ssize_t status = 0;

        do {
            status = ::recv(s, buf, len, flags);
        } while ((status < 0) && (get_socket_error() == EINTR));

        return status;
    }

    ssize_t
        send_ignore_interrupts(ARRAS_SOCKET s, const char* buf, size_t len, int flags)
    {
        ssize_t status = 0;

        do {
            status = ::send(s, buf, len, flags);
        } while ((status < 0) && (get_socket_error() == EINTR));

        return status;
    }

#ifdef USE_POLL
    // this version of poll won't handle the timeout properly since the
    // call will be remade with the entire timeout properly. 0 (return immediately)
    // and -1 (no timeout) will work the same though.
    int
        poll_ignore_interrupts(struct pollfd* fds, nfds_t nfds, int timeout)
    {
        int status = 0;

        do {
            status = ::poll(fds, nfds, timeout);
        } while ((status < 0) && (get_socket_error() == EINTR));

        return status;
    }
#else 
    int
        select_ignore_interrupts(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const timeval* timeout)
    {
        int status = 0;

        do {
            status = ::select(nfds, readfds, writefds, exceptfds, timeout);
        } while ((status < 0) && (get_socket_error() == EINTR));

        return status;
    }
#endif

#ifdef PLATFORM_LINUX
    int accept_ignore_interrupts(ARRAS_SOCKET s, struct sockaddr* addr, socklen_t* addrlen)
    {
        int status = 0;

        do {
            status = ::accept4(s, addr, addrlen, SOCK_CLOEXEC);
        } while ((status < 0) && (get_socket_error() == EINTR));

        return status;
    }
#else
    int accept_ignore_interrupts(ARRAS_SOCKET s, struct sockaddr* addr, socklen_t* addrlen)
    {
        int status = 0;

        do {
            status = ::accept(s, addr, addrlen);
        } while ((status < 0) && (get_socket_error() == EINTR));

        return status;
    }
#endif

}

namespace arras4 {
    namespace network {

        // strerror isn't thread safe because it uses an internal buffer for
        // returning the string so we need to use strerror_r which takes a
        // user provided buffer. Rather than worry about allocating and
        // freeing a char array just stuff it into a std::string.
        std::string SocketPeer::getErrorString(int aErrno)
        {
            char buffer[200];
            buffer[0] = 0;
#ifdef PLATFORM_WINDOWS
            // no strerror_r() on Windows, but strerror() is threadsafe
            return std::string(strerror(aErrno));
#elif defined PLATFORM_LINUX
            return std::string(strerror_r(aErrno, buffer, sizeof(buffer)));
#elif defined PLATFORM_APPLE
            int err = strerror_r(aErrno, buffer, sizeof(buffer));
            return std::string(buffer);
#endif
        }
        
        int SocketPeer::getSocketError() {
            return get_socket_error();
        }

        int SocketPeer::getLookupError()
        {
#ifdef PLATFORM_WINDOWS
            return WSAGetLastError();
#else
            return h_errno;
#endif
        }

        PeerException::Code SocketPeer::getCodeFromSocketError(int aErrno)
        {
            switch (aErrno) {
#ifdef PLATFORM_WINDOWS
            case WSAEAFNOSUPPORT: return PeerException::UNSUPPORTED_ADDRESS_FAMILY;
            case WSANOTINITIALISED: return PeerException::NOT_INITIALIZED;
            case WSAETIMEDOUT: return PeerException::TIMEOUT;
            case WSAECONNABORTED: return PeerException::CONNECTION_ABORT;
            case WSAENOTSOCK: return PeerException::INVALID_DESCRIPTOR;
            case WSAHOST_NOT_FOUND: return PeerException::NO_HOST;
            case WSANO_RECOVERY: return PeerException::NAME_SERVER_ERROR;
            case WSATRY_AGAIN: return PeerException::NAME_SERVER_ERROR;
#else
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
#endif
            default: return PeerException::UNKNOWN;
            }
        }

        PeerException::Code
            SocketPeer::getCodeFromLookupError(int aErrno)
        {
#ifdef PLATFORM_WINDOWS
            return getCodeFromSocketError(aErrno);
#else
            switch (aErrno) {

            case HOST_NOT_FOUND: return PeerException::NO_HOST;
            case NO_ADDRESS: return PeerException::ADDRESS_NOT_FOUND;
            case NO_RECOVERY: return PeerException::NAME_SERVER_ERROR;
            case TRY_AGAIN: return PeerException::NAME_SERVER_ERROR;
            default: return PeerException::UNKNOWN;
            }
#endif
        }

        PeerException::Code
            SocketPeer::getCodeFromGetAddrInfoError(int aErrno)
        {
#ifdef PLATFORM_WINDOWS
            return getCodeFromSocketError(aErrno);
#else
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
#endif
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
#ifndef PLATFORM_WINDOWS
            // check if "sock" is a socket (not possible on Windows)
            struct stat stat_info;
            if (fstat(sock, &stat_info) < 0) {
                int save_errno = errno;

                if (save_errno == EBADF) {
                    throw InvalidParameterError("Bad file descriptor");
                }
                else {
                    throw InvalidParameterError("Problem calling fstat on file descriptor");
                }
            }

            if (!S_ISSOCK(stat_info.st_mode)) {
                throw InvalidParameterError("File descriptor is not a socket");
            }
#endif

#ifdef PLATFORM_APPLE

            // Check UNIX sockets 
            pid_t pid;
            struct socket_fdinfo info;
            int size;

            pid = getpid();

            size = proc_pidfdinfo(pid, PROC_PIDFDSOCKETINFO, sock, &info, PROC_PIDFDSOCKETINFO_SIZE);
            if (size == PROC_PIDFDSOCKETINFO_SIZE) {
                if ((info.psi.soi_options & SO_ACCEPTCONN) == SO_ACCEPTCONN) {
                    mIsListening = true;
                }    
            }    

            // Check TCP sockets 
	        tcp_connection_info infoTCP;
            socklen_t sizeTCP = sizeof(infoTCP); 

	        if (getsockopt(sock, IPPROTO_TCP, TCP_CONNECTION_INFO, (void *) &infoTCP, &sizeTCP) != 0) {
                // save errno before doing anything else
                int save_errno = errno;
                std::string err("SocketPeer: Apple Problem calling getsockopt on the socket ");
                err += getErrorString(save_errno);
	        } else { 
                if ( infoTCP.tcpi_state == TCPS_LISTEN ) {
                    mIsListening = true;
                }
            } 
#else
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
            }
            else {
                mIsListening = isListening;
            }
#endif
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
#ifdef PLATFORM_WINDOWS
            ::shutdown(mSocket, SD_BOTH);
#else
            ::shutdown(mSocket, SHUT_RDWR);
#endif

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
#ifdef PLATFORM_WINDOWS
            ::shutdown(mSocket, SD_SEND);
#else
            ::shutdown(mSocket, SHUT_WR);
#endif
        }

        void
            SocketPeer::shutdown_receive()
        {
            if (mSocket == ARRAS_INVALID_SOCKET) {
                throw InvalidParameterError("Attempted shutdown_send on an uninitialized peer");
            }
#ifdef PLATFORM_WINDOWS
            ::shutdown(mSocket, SD_RECEIVE);
#else
            ::shutdown(mSocket, SHUT_RD);
#endif
        }

        bool
            SocketPeer::send(const void* data, size_t nBytes)
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
            int flags =
#ifdef PLATFORM_WINDOWS
                0; // Tried SO_NOSIGPIPE; but this was tripping up the socket connections
#else
                MSG_NOSIGNAL;
#endif

            // if there is an encryption object then delegate the write
            if (mEncryption != nullptr) {
                bool rtn = 0;

                try {
                    rtn = mEncryption->write(data, nBytes);
                }
                catch (const EncryptException& e) {
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
                    }
                    else {
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
            SocketPeer::receive(void* buffer, size_t nMaxBytesToRead)
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
                }
                catch (const EncryptException& e) {
                    throw PeerException(e.what());
                }
                if (rtn < 0) {
                    throw PeerException("SocketPeer::receive error doing encrypted read");
                }
                mBytesRead += rtn;
                return rtn;
            }

            int flags =
#ifdef PLATFORM_WINDOWS
                0; // Tried SO_NOSIGPIPE; but this was tripping up the socket connections
#else
                MSG_NOSIGNAL;
#endif

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
                    }
                    else {
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
                }
                catch (PeerDisconnectException&) {
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
                    }
                    else {
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
#ifdef PLATFORM_WINDOWS
                ::shutdown(mSocket, SD_BOTH);
#else
                ::shutdown(mSocket, SHUT_RDWR);
#endif
            }
        }

        size_t
            SocketPeer::peek(void* buffer, size_t nMaxBytesToRead)
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
                }
                catch (const EncryptException& e) {
                    throw PeerException(e.what());
                }

                if (rtn < 0) {
                    throw PeerException("SocketPeer::peek error doing encrypted read");
                }

                return rtn;
            }
            int flags =
#ifdef PLATFORM_WINDOWS
                0; // Tried SO_NOSIGPIPE; but this was tripping up the socket connections
#else
                MSG_NOSIGNAL;
#endif

            ssize_t rtn = recv_ignore_interrupts(mSocket, (char*)buffer, nMaxBytesToRead, flags | MSG_PEEK);
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

#if defined(USE_POLL)
// This version of SocketPeer::poll() uses the system poll() function, which is not available on Windows
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
                }
                catch (const EncryptException& e) {
                    throw PeerException(e.what());
                }
            }

            int r = 0; // return value of poll()   

            if (timeoutMs == 0) {

                // timeout of 0 causes the function to return immediately
                r = poll_ignore_interrupts(&pfd, 1, 0);

                // if r==0 we didn't find anything
                if (r == 0) return false;

            }
            else {

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
                    if (r == 0) {
                        return false;
                    }
                    // check for error EINTR ( = "a signal occured before timeout elapsed")
                    else if ((r == -1) && (errno == EINTR)) {

                        // calculate how many milliseconds of timeout remain
                        std::chrono::steady_clock::duration left = endTime - std::chrono::steady_clock::now();
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

            }
            else {

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
                        std::chrono::steady_clock::duration left = endTime - std::chrono::steady_clock::now();
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

#else // ifdef USE_POLL
// These versions of poll and accept use the 'select' system function : 'poll' is generally preferred, but is not
// available on the Windows platform

        bool
            SocketPeer::poll(
                bool query_read, bool& read,
                bool query_write, bool& write,
                unsigned int timeoutMs)
        {
            // clear them
            if (query_read) read = false;
            if (query_write) write = false;
            bool returnValue = false;

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

            // poll the socket to see if anything is waiting
            fd_set rd, wr;
            FD_ZERO(&rd);
            FD_ZERO(&wr);

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
                }
                catch (const EncryptException& e) {
                    throw PeerException(e.what());
                }
            }


            if (query_read)  FD_SET(mSocket, &rd);
            if (query_write) FD_SET(mSocket, &wr);

            struct timeval remaining;
            int r = 0;
            if (timeoutMs == 0) {
                remaining.tv_sec = 0;
                remaining.tv_usec = 0;
                r = select_ignore_interrupts(mSocket + 1, &rd, &wr, nullptr, &remaining);
            }
            else {
                struct timeval endtime;
                struct timeval now;

                // select can return before the timeout when there is a signal
                // so to honor the timeout we need to loop on the call to select
                // using a newly computed timeout each time

                // initial timeout amount
                remaining.tv_sec = timeoutMs / 1000;
                remaining.tv_usec = (timeoutMs % 1000) * 1000;

                // compute the target timeout time
                gettimeofday(&endtime, nullptr);
                timeradd(&endtime, &remaining, &endtime);

                do {
                    // don't use selectt_ignore_interrupts() because we have a timeout to deal with
                    r = ::select(mSocket + 1, &rd, &wr, nullptr, &remaining);

                    // weird error or normal return exit the loop
                    if ((r >= 0) || (errno != EINTR)) break;

                    // the select returned early due to signal, compute a new remaining time
                    gettimeofday(&now, nullptr);

                    // have we already reached the specified timeout anyway?
                    // timercmp is a macro which is why it can have the weird
                    // syntax with ">" as a parameter
                    if (timercmp(&now, &endtime, > )) break;

                    // how much time remains for timeout
                    timersub(&endtime, &now, &remaining);

                } while (1);
                {
                    gettimeofday(&now, nullptr);

                    // have we already reached the specified timeout anyway?
                    // timercmp is a macro which is why it can have the weird
                    // syntax with ">" as a parameter
                    if (timercmp(&now, &endtime, > )) {
                    }
                    else {

                        // how much time remains for timeout
                        timersub(&endtime, &now, &remaining);
                    }
                }
            }

            // these non-EINTR errors should really only be programming errors
            if (r == -1) {
                int save_errno = getSocketError();

                // this shouldn't really reach here but check just in case
                if (save_errno == EINTR) return false;

                // throw an exception
                shutdown();
                std::string err("SocketPeer::poll: ");
                err += getErrorString(save_errno);
                throw PeerException(save_errno, getCodeFromSocketError(save_errno), err.c_str());
            }

            // the timeout was reached without the status changing on the selected I/O type
            // just use the false values that have already been set
            // If read information wasn't requested we won't have any information about whether the
            // remote connection closed its writes so just return false. If reads weren't requested
            // then the caller probably isn't about to do a read anyway
            if (r == 0) return false;

            // select will only indicate that a read won't block but won't notice that the connection
            // is being closed down. If it claims that reads are possible then try a peek to find
            // out whether the connection is closing down
            if (query_read && FD_ISSET(mSocket, &rd)) {
                unsigned char p[1];

                // to match select() behavior we don't care WHY recv wouldn't block
                // to know that there is actually data to be read look for a true read value
                // and a false return value
                read = true;

                // the cast of p is needed for WINDOWS and harmless for others
                ssize_t n = ::recv(mSocket, (char*)p, 1, MSG_PEEK);
                // windows returns -1 rather than 0 when the socket is shutdown
#ifndef _WINDOWS)
                if (n < 0) {

                    // save errno before doing anything else
                    int save_errno = errno;

                    // clean up
                    shutdown();

                    // throw an exception
                    std::string err("SocketPeer::poll: ");
                    err += getErrorString(save_errno);
                    throw PeerException(save_errno, getCodeFromSocketError(save_errno), err.c_str());
                }
#endif

#ifdef PLATFORM_WINDOWS
                if (n <= 0) {
#else
                if (n == 0) {
#endif
                    // the remote endpoint has closed so return true to let the caller know
                    // that the connection is closing down
                    returnValue = true;
                }
                }

            if (query_write && FD_ISSET(mSocket, &wr)) {
                write = true;
            }

            return returnValue;
            }

        void
            SocketPeer::accept(Peer * *&peers, int& nPeers, unsigned int timeoutMs)
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

            fd_set fds;
            FD_ZERO(&fds);

            // add the main listening socket
            FD_SET(mSocket, &fds);

            struct timeval remaining;
            int r = 0;
            if (timeoutMs == 0) {
                remaining.tv_sec = 0;
                remaining.tv_usec = 0;
                // we ignore write FDs since we use blocking writes on client sockets
                r = select_ignore_interrupts(mSocket + 1, &fds, nullptr, nullptr, &remaining);
            }
            else {
                struct timeval endtime;
                struct timeval now;

                // accept can return before the timeout when there is a signal
                // so to honor the timeout we need to loop on the call to select
                // using a newly computed timeout each time

                // initial timeout amount
                remaining.tv_sec = timeoutMs / 1000;
                remaining.tv_usec = (timeoutMs % 1000) * 1000;

                // compute the target timeout time
                gettimeofday(&endtime, nullptr);
                timeradd(&endtime, &remaining, &endtime);

                do {

                    // we ignore write FDs since we use blocking writes on client sockets
                    // don't use select_ignore_interrupts() because we have a timeout to deal with
                    r = ::select(mSocket + 1, &fds, nullptr, nullptr, &remaining);

                    // weird error or normal return exit the loop
                    if ((r >= 0) || (errno != EINTR)) break;

                    // the select returned early due to signal, compute a new remaining time
                    gettimeofday(&now, nullptr);

                    // have we already reached the specified timeout anyway?
                    // timercmp is a macro which is why it can have the weird
                    // syntax with ">" as a parameter
                    if (timercmp(&now, &endtime, > )) break;

                    // how much time remains for timeout
                    timersub(&endtime, &now, &remaining);

                } while (1);
            }

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

            // one or more new incoming connections are available, accept
            // them all (up to the number of peers provided)
            if (FD_ISSET(mSocket, &fds)) {
                acceptAll(peers, nPeers);
            }
        }

#endif // ifdef USE_POLL


        void
            SocketPeer::acceptAll(Peer * *&peers, int& nPeers)
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

                newSocket = accept_ignore_interrupts(mSocket, nullptr, nullptr);

                // there was an error
                if (newSocket == ARRAS_INVALID_SOCKET) {
                    // save errno before doing anything else
                    int save_errno = getSocketError();

                    // EWOULDBLOCK would indicate that there is
                    // nothing to accept though if everyone is using this
                    // correctly we should already know that there are connections
                    // to be accepted.
#ifdef PLATFORM_WINDOWS
                    if (save_errno != WSAEWOULDBLOCK) {
#else
                    if ((save_errno != EWOULDBLOCK) && (save_errno != EAGAIN)) {
#endif
                        // throw an exception
                        std::string err("SocketPeer::acceptAll: ");
                        err += getErrorString(save_errno);
                        throw PeerException(save_errno, getCodeFromSocketError(save_errno), err);
                    }
                    return;
                    }

                // a non-negative value is a file descriptor for a socket

                // we want blocking IO on the client sockets (for write)
#ifdef PLATFORM_WINDOWS
                unsigned long no = 0;
                int status = ioctlsocket(newSocket, FIONBIO, &no);
                if (status < 0) {
                    close_ignore_interrupts(newSocket);
#else
                int no = 0;
                if (ioctl(newSocket, FIONBIO, (char*)&no) < 0) {
                    close_ignore_interrupts(newSocket);
#endif
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

#ifdef PLATFORM_WINDOWS
            //Random, useless pointers for WinSock call
            DWORD bytesReturned;
            WSAOVERLAPPED overlapped;
            overlapped.hEvent = NULL;
            //Set KeepAlive settings -- I HATE WINSOCK
            struct tcp_keepalive settings;
            settings.onoff = 1;
            settings.keepalivetime = keepTime * 1000;
            settings.keepaliveinterval = keepInvl * 1000;
            WSAIoctl(mSocket, SIO_KEEPALIVE_VALS, &settings, sizeof(struct tcp_keepalive), NULL, 0, &bytesReturned, &overlapped, NULL);
#endif

#ifdef PLATFORM_LINUX
            setsockopt(mSocket, IPPROTO_TCP, TCP_KEEPIDLE, &keepTime, sizeof(keepTime));
            setsockopt(mSocket, IPPROTO_TCP, TCP_KEEPINTVL, &keepInvl, sizeof(keepInvl));
            setsockopt(mSocket, IPPROTO_TCP, TCP_KEEPCNT, &keepProbe, sizeof(keepProbe));
#endif
        }

            } // namespace network
        } // namespace arras
