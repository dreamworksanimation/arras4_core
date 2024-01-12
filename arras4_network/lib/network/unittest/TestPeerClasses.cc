// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "TestPeerClasses.h"
#include "TestTimer.h"

#include <network/Peer.h>
#include <network/SocketPeer.h>
#include <network/InetSocketPeer.h>
#include <network/IPCSocketPeer.h>
#include <network/InvalidParameterError.h>

#include <atomic>
#include <fcntl.h>
#include <limits>
#include <signal.h>
#include <string.h>
#include <thread>

#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__ICC)
#define ARRAS_START_THREADSAFE_STATIC_WRITE       __pragma(warning(disable:1711))
#define ARRAS_FINISH_THREADSAFE_STATIC_WRITE __pragma(warning(default:1711))
#else
#define ARRAS_START_THREADSAFE_STATIC_WRITE
#define ARRAS_FINISH_THREADSAFE_STATIC_WRITE
#endif

// #define TRACE fprintf(stderr,"TRACE %d\n", __LINE__)
#define TRACE
const unsigned int STANDARD_PORT=9001;
const char STANDARD_NAME[] =  "/tmp/testpeerclasses_simple_name";
const char LONG_NAME_107[] ="/tmp/check_that_a_107_character_name_is_accepted_as_an_IPC_name_since_that_is_a_length_which_will_fit______";
std::atomic<bool> hangServer;
std::atomic<int> serverCommand;

// commands
int SERVER_SHUTDOWN_SOCKET = 1;
int SERVER_SEND_16 = 2;
int SERVER_RELEASE_AFTER_10_SECONDS = 3;
int SERVER_SHUTDOWN_SOCKET_SEND = 4;
int SERVER_SEND_16_SLOWLY = 5;
int SERVER_SEND_16_AFTER_10_SECONDS = 6;
char STANDARD_STRING16[17] = "0123456789012345";

CPPUNIT_TEST_SUITE_REGISTRATION(TestPeerClasses);

using arras4::network::Peer;
using arras4::network::IPCSocketPeer;
using arras4::network::InetSocketPeer;
using arras4::network::SocketPeer;
using arras4::network::InvalidParameterError;
using arras4::network::PeerException;
using arras4::ARRAS_INVALID_SOCKET;

bool serverDone = false;
char* serverIPCName = nullptr;
Peer* serverPeer;
bool isShutdown = false;

void setIsShutdown(bool value)
{
ARRAS_START_THREADSAFE_STATIC_WRITE
    isShutdown = value;
ARRAS_FINISH_THREADSAFE_STATIC_WRITE
}


void
handleServerCommand()
{
    if (serverCommand == SERVER_SHUTDOWN_SOCKET) {
        serverPeer->shutdown();
        serverCommand = 0;
        setIsShutdown(true);
    } else if (serverCommand == SERVER_SEND_16) {
        serverPeer->send(STANDARD_STRING16, 16);
        serverCommand = 0;
    } else if (serverCommand == SERVER_SEND_16_SLOWLY) {
        serverCommand = 0;
        for (int i=0; i<16; i++) {
            sleep(1);
            serverPeer->send(&STANDARD_STRING16[i], 1);
        }
    } else if (serverCommand == SERVER_RELEASE_AFTER_10_SECONDS) {
        sleep(10);
        hangServer = false;
        serverCommand = 0;
    } else if (serverCommand == SERVER_SEND_16_AFTER_10_SECONDS) {
        serverCommand = 0;
        sleep(10);
        serverPeer->send(STANDARD_STRING16, 16);
    } else if (serverCommand == SERVER_SHUTDOWN_SOCKET_SEND) {
        serverPeer->shutdown_send();
        serverCommand = 0;
    }
}

void
sendServerCommand(int command)
{
    serverCommand = command;
    while (serverCommand != 0) {
        sleep(1);
    }
}

void
inetPeerServerThread()
{
    hangServer = false;
    serverCommand = 0;
    InetSocketPeer* peer = new InetSocketPeer();
    CPPUNIT_ASSERT( peer != nullptr);

    // set up a simple listener
    peer->listen(STANDARD_PORT, 1);

    while (1) {
        Peer** peerPtr = &serverPeer;
        int nPeers;

        TRACE;
        // wait for a connection
        do {
            // TRACE;
            while (hangServer) {
                TRACE;
                handleServerCommand();
                sleep(1);
            }
            // need to set the count every time since it is also a return value
            nPeers = 1;
            try {
                peer->accept(peerPtr, nPeers, 100);
            } catch (...) {
                TRACE;
                // re-throw to get message text from current exception
                std::string msg("inetPeerServerThread ");
                std::exception_ptr p = std::current_exception();
                try {
                    std::rethrow_exception(p);
                } catch (const std::exception& e) {
		    // get exception text
                    msg += e.what();
                }
                msg += " Exception while doing accept in server thread\n";
                CPPUNIT_ASSERT(false && msg.c_str());
	    }

            if (serverDone) {
	        TRACE;
                delete peer;
	        TRACE;
                return;
            }
        } while (nPeers == 0);

        do {
            TRACE;
            setIsShutdown(false);
            TRACE;
	    if (hangServer) {
	        TRACE;
                while (hangServer) {
                    handleServerCommand();
                    sleep(1);
                }
                TRACE;
            }
            TRACE;
            char buffer[100];
            // wait for data to be available
            bool read_status;
            bool write_status;

            handleServerCommand();

            // make sure there is something to read before reading
            do {
                TRACE;
                try {
                    bool return_status = serverPeer->poll(true, read_status, false, write_status, 100);
                    if (return_status == true) {
		        TRACE;
                        setIsShutdown(true);
		        TRACE;
                    } else {
		        // cyu: "TIMED OUT (over 600s)" with read_status 0
                        // could be poll() timed out 
                        TRACE;
		        fprintf(stderr,"TRACE %d %d\n", __LINE__,read_status);
                    }
                } catch (...) {
		    TRACE;
                    setIsShutdown(true);
		    TRACE;
                }
                handleServerCommand();
            } while (!read_status && !isShutdown);

            // now that we know that there us at least one byte to read we can go ahead and read
            if (!isShutdown) {
                try {
                    // TRACE;
                    if (serverPeer->receive(buffer, 100) == 0) {
		        TRACE;
                        setIsShutdown(true);
		        TRACE;
                    }
                } catch (...) {
		    TRACE;
                    setIsShutdown(true);
		    TRACE;
                }
                // TRACE;
            }
            if (serverDone) {
                TRACE;
                delete peer;
                return;
            }
        } while (!isShutdown);

        TRACE;
        delete serverPeer;
        TRACE;
    }
}

void ipcPeerServerThread()
{
    IPCSocketPeer* peer = new IPCSocketPeer();
    CPPUNIT_ASSERT( peer != nullptr);

    // set up a simple listener
    peer->listen(serverIPCName);

    while (1) {
       Peer** peerPtr = &serverPeer;
        int nPeers;

        // wait for a connection
        do {
            // need to set the count every time since it is also a return value
            nPeers = 1;
            try {
                peer->accept(peerPtr, nPeers, 100);
            } catch (...) {
	        // re-throw to get message text from current exception
                std::string msg("ipcPeerServerThread ");
                msg += std::to_string(peer->fd());
                msg += " ";
                msg += serverIPCName;
                msg += ", nPeers=";
                msg += std::to_string(nPeers);
                msg += " ";
                std::exception_ptr p = std::current_exception();
                try {
                    std::rethrow_exception(p);
                } catch (const std::exception& e) {
		    // get exception text
		    msg += e.what();
                }
                msg += " Exception while doing accept in server thread\n";
                CPPUNIT_ASSERT(false && msg.c_str());
            }
            if (serverDone) return;
        } while (nPeers == 0);

        setIsShutdown(false);
        do {
            char buffer[100];
            try {
                if (serverPeer->receive(buffer, 100) == 0) {
                    setIsShutdown(true);
                }
            } catch (...) {
                setIsShutdown(true);
            }
            if (serverDone) return;
        } while (!isShutdown);

        delete serverPeer;
    }
}

std::thread listenerThread;

void
startInetServer()
{
ARRAS_START_THREADSAFE_STATIC_WRITE
    serverDone = false;
ARRAS_FINISH_THREADSAFE_STATIC_WRITE
    listenerThread = std::thread(inetPeerServerThread);
    sleep(1);
}

void
stopInetServer()
{
    TRACE;
ARRAS_START_THREADSAFE_STATIC_WRITE
    serverDone = true;
    TRACE;
ARRAS_FINISH_THREADSAFE_STATIC_WRITE
    if (listenerThread.joinable()) {
        TRACE;
	sleep(3);
        listenerThread.join();
        TRACE;
    }
}

void
startIPCServer(const char* aIPCName)
{
    if (serverIPCName != nullptr) {
        free(serverIPCName);
    }
ARRAS_START_THREADSAFE_STATIC_WRITE
    serverIPCName = strdup(aIPCName);
    serverDone = false;
ARRAS_FINISH_THREADSAFE_STATIC_WRITE
    listenerThread = std::thread(ipcPeerServerThread);
    sleep(1);
}

void
stopIPCServer()
{
ARRAS_START_THREADSAFE_STATIC_WRITE
    serverDone = true;
ARRAS_FINISH_THREADSAFE_STATIC_WRITE
    if (listenerThread.joinable()) listenerThread.join();
}

void TestPeerClasses::testSocketPeerAccept()
{
    Peer* peers[32];
    Peer** peerPtr = peers;
    int nPeers;
    InetSocketPeer* inetPeer;


    TRACE;
    //
    // do accept() and acceptAll() on an unitialized SocketPeer
    //
    {
        inetPeer = new InetSocketPeer();
        try {
            nPeers = 32;
            inetPeer->accept(peerPtr, nPeers, 1000);
            CPPUNIT_ASSERT(false && "didn't get exception doing accept on a uninitialized socket");
        } catch (InvalidParameterError&) {
        }
        delete inetPeer;
    }
    {
        inetPeer = new InetSocketPeer();
        try {
            nPeers = 32;
            inetPeer->acceptAll(peerPtr, nPeers);
            CPPUNIT_ASSERT(false && "didn't get exception doing acceptAll on a uninitialized socket");
        } catch (InvalidParameterError&) {
        }
        delete inetPeer;
    }

    TRACE;
    startInetServer();

    TRACE;
    //
    // do accept() and acceptAll() on a SocketPeer which isn't listening
    //
    {
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        try {
            nPeers = 32;
            inetPeer->accept(peerPtr, nPeers, 1000);
            CPPUNIT_ASSERT(false && "didn't get exception doing accept on a non listening socket");
        } catch (InvalidParameterError&) {
        }
        delete inetPeer;
    }
    TRACE;
    {
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        try {
            nPeers = 32;
            inetPeer->acceptAll(peerPtr, nPeers);
            CPPUNIT_ASSERT(false && "didn't get exception doing acceptAll on a non listening socket");
        } catch (InvalidParameterError&) {
        }
        delete inetPeer;
    }

    TRACE;
    stopInetServer();

    //
    // test for failure due to exhausted file descriptors
    //
    {
        InetSocketPeer* listenerPeer = new InetSocketPeer();

        listenerPeer->listen(STANDARD_PORT);

        inetPeer = new InetSocketPeer();
        CPPUNIT_ASSERT(inetPeer);
        inetPeer->connect("localhost", STANDARD_PORT);

        struct rlimit oldlimit;
        struct rlimit newlimit;
        CPPUNIT_ASSERT(getrlimit(RLIMIT_NOFILE, &oldlimit) == 0);
        newlimit.rlim_max = oldlimit.rlim_max;
        newlimit.rlim_cur = 10;
        setrlimit(RLIMIT_NOFILE, &newlimit);

        // burn the few remaining file handles
        int handles[20];
        int index=0;
        for (index=0; index < 20; index++) {
            handles[index]= open("/dev/null", O_RDONLY);
            if (handles[index] < 0) break;
        }
        // make sure we actually hit a limit
        CPPUNIT_ASSERT(index < 20);
   
        try {
            nPeers = 32;
            listenerPeer->accept(peerPtr, nPeers, 1000);
            CPPUNIT_ASSERT(false && "didn't get exception doing accept with no file descriptors available");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EMFILE);
            CPPUNIT_ASSERT(e.code() == PeerException::FILES);
        }

        // free up the file handles
        for (int i = 0; i < index; i++) {
            CPPUNIT_ASSERT(close(handles[i]) == 0);
        }
        // put the file handle limit back the way it was
        setrlimit(RLIMIT_NOFILE, &oldlimit);

        // now go ahead and finish the accept
        nPeers = 32;
        listenerPeer->accept(peerPtr, nPeers, 1000);
        CPPUNIT_ASSERT(nPeers == 1);
        delete peerPtr[0];
        delete inetPeer;

        inetPeer = new InetSocketPeer();
        CPPUNIT_ASSERT(inetPeer);
        inetPeer->connect("localhost", STANDARD_PORT);

        CPPUNIT_ASSERT(getrlimit(RLIMIT_NOFILE, &oldlimit) == 0);
        newlimit.rlim_max = oldlimit.rlim_max;
        newlimit.rlim_cur = 10;
        setrlimit(RLIMIT_NOFILE, &newlimit);

        // burn the few remaining file handles
        index=0;
        for (index=0; index < 20; index++) {
            handles[index]= open("/dev/null", O_RDONLY);
            if (handles[index] < 0) break;
        }
        // make sure we actually hit a limit
        CPPUNIT_ASSERT(index < 20);
   
        try {
            nPeers = 32;
            listenerPeer->acceptAll(peerPtr, nPeers);
            CPPUNIT_ASSERT(false && "didn't get exception doing acceptAll with no file descriptors available");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EMFILE);
            CPPUNIT_ASSERT(e.code() == PeerException::FILES);
        }

        // free up the file handles
        for (int i = 0; i < index; i++) {
            CPPUNIT_ASSERT(close(handles[i]) == 0);
        }
        // put the file handle limit back the way it was
        setrlimit(RLIMIT_NOFILE, &oldlimit);

        delete listenerPeer;

    }
    TRACE;

    //
    // Test that accept() timeout is honored even when there are alarm() signals
    // (acceptAll() doesn't do timeouts)
    //
    {
        InetSocketPeer* listenerPeer = new InetSocketPeer();
        TRACE;

        listenerPeer->listen(STANDARD_PORT);
        TRACE;

        nPeers = 32;
        TestTimer timer;
        timer.start();
        alarm(2);
        TRACE;
        listenerPeer->accept(peerPtr, nPeers, 1000);
        TRACE;
        timer.stop();
        CPPUNIT_ASSERT(timer.timeElapsed() > 0);
        CPPUNIT_ASSERT(timer.timeElapsed() < 2.0);
        CPPUNIT_ASSERT(nPeers == 0);

        delete listenerPeer;
    }
    TRACE;

    //
    // call acceptAll directly without any connections waiting
    //
    {
        InetSocketPeer* listenerPeer = new InetSocketPeer();

    TRACE;
        listenerPeer->listen(STANDARD_PORT);
    TRACE;

        nPeers = 32;
        listenerPeer->acceptAll(peerPtr, nPeers);
    TRACE;
        CPPUNIT_ASSERT(nPeers == 0);

        delete listenerPeer;
    }
    TRACE;

    //
    // call accept when there are more connections waiting than in space in the array
    //
    {
        InetSocketPeer* listenerPeer = new InetSocketPeer();

        listenerPeer->listen(STANDARD_PORT);

        Peer* connections[10];
        for (int i = 0; i < 10; i++) {
            connections[i]= new InetSocketPeer("localhost", STANDARD_PORT); 
            CPPUNIT_ASSERT(connections[i] != nullptr);
        }

        nPeers = 4;
        listenerPeer->accept(peerPtr, nPeers, 1000);
        CPPUNIT_ASSERT(nPeers == 4);
        for (int i = 0; i < 4; i++) {
            delete connections[i];
            delete peerPtr[i];
        }
        nPeers = 4;
        listenerPeer->accept(peerPtr, nPeers, 1000);
        CPPUNIT_ASSERT(nPeers == 4);
        for (int i = 0; i < 4; i++) {
            delete connections[i+4];
            delete peerPtr[i];
        }
        nPeers = 4;
        listenerPeer->accept(peerPtr, nPeers, 1000);
        CPPUNIT_ASSERT(nPeers == 2);
        for (int i = 0; i < 2; i++) {
            delete connections[i+8];
            delete peerPtr[i];
        }

        delete listenerPeer;
    }
    TRACE;
}

void TestPeerClasses::testSocketPeerPeek()
{
    InetSocketPeer* inetPeer = new InetSocketPeer();

    //
    // try peek on an uninitialized InetSocketPeer
    //
    TRACE;
    char buffer[17];
    try {
        inetPeer->peek(buffer, 16);
    } catch (PeerException& e) {
        CPPUNIT_ASSERT(e.error() == EBADF);
        CPPUNIT_ASSERT(e.code() == PeerException::INVALID_DESCRIPTOR);
    }

    //
    // try receiving on a listening socket
    //
    TRACE;
    inetPeer = new InetSocketPeer();
    inetPeer->listen(STANDARD_PORT);
    try {
        inetPeer->peek(buffer, 16);
        CPPUNIT_ASSERT(false && "didn't get exception for peek on a listening InetSocketPeer");
    } catch (InvalidParameterError&) {
    }
    delete inetPeer;

    startInetServer();

    //
    // cause a SIGALRM interrupt to happen while doing a peek
    //
    {
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(inetPeer != nullptr);

        memset(buffer, 0, 17);

        sendServerCommand(SERVER_SEND_16_AFTER_10_SECONDS);
        sleep(1);

        alarm(2);
        // keep peeking until all of the data arrives
        size_t return_status = 0;
        do {
            return_status = inetPeer->peek(buffer, 16);
            CPPUNIT_ASSERT(return_status > 0);
        } while (return_status != 16); 

        // now go ahead and read the data
        size_t offset = 0;
        size_t count = 16;
        while (count > 0) {
            return_status = inetPeer->receive(&buffer[offset], count);
            CPPUNIT_ASSERT(return_status > 0);
            offset += return_status;
            count -= return_status;
        }
        CPPUNIT_ASSERT(strcmp(buffer,STANDARD_STRING16) == 0);
        delete inetPeer;
    }

    stopInetServer();
}

void TestPeerClasses::testSocketPeerReceive()
{
    InetSocketPeer* inetPeer = nullptr;

    TRACE;
    //
    // try receive() and receive_all() on an uninitialized InetSocketPeer
    //
    {
        char buffer[17];
        inetPeer = new InetSocketPeer();
        try {
            inetPeer->receive(buffer,16);
            CPPUNIT_ASSERT(false && "didn't get exception for receive on uninitialized InetSocketPeer");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EBADF);
            CPPUNIT_ASSERT(e.code() == PeerException::INVALID_DESCRIPTOR);
        }
        try {
            inetPeer->receive_all(buffer,16);
            CPPUNIT_ASSERT(false && "didn't get exception for receive_all on uninitialized InetSocketPeer");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EBADF);
            CPPUNIT_ASSERT(e.code() == PeerException::INVALID_DESCRIPTOR);
        }
        delete inetPeer;
    }

    TRACE;
    //
    // try receive() and receive_all() on a listening socket
    //
    {
        char buffer[17];
        inetPeer = new InetSocketPeer();
        inetPeer->listen(STANDARD_PORT);
        try {
            inetPeer->receive(buffer, 16);
            CPPUNIT_ASSERT(false && "didn't get exception for receive on a listening InetSocketPeer");
        } catch (InvalidParameterError&) {
        }
        try {
            inetPeer->receive_all(buffer, 16);
            CPPUNIT_ASSERT(false && "didn't get exception for receive_all on a listening InetSocketPeer");
        } catch (InvalidParameterError&) {
        }
        delete inetPeer;
    }

    startInetServer();


    TRACE;
    //
    // receive_all() 0 bytes
    //
    {
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(inetPeer != nullptr);

        char buffer[17];
        try {
            inetPeer->receive(buffer, 0);
            CPPUNIT_ASSERT(false && "didn't get exception for receive of 0 bytes");
        } catch (InvalidParameterError&) {
        }

        // receive_all allows 0 bytes since the return is a boolean
        CPPUNIT_ASSERT(inetPeer->receive_all(buffer, 0) == true);

        delete inetPeer;
    }


    TRACE;
    //
    // receive() and receive_all() to an null data ptr
    //
    {
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(inetPeer != nullptr);

        try {
            inetPeer->receive(nullptr, 16);
            CPPUNIT_ASSERT(false && "didn't get exception for receive using nullptr");
        } catch (InvalidParameterError&) {
        }
        try {
            inetPeer->receive_all(nullptr, 16);
            CPPUNIT_ASSERT(false && "didn't get exception for receive_all using nullptr");
        } catch (InvalidParameterError&) {
        }

        delete inetPeer;
    }

    TRACE;
    //
    // do a basic functioning receive
    //
    {
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(inetPeer != nullptr);

        char buffer[17];
        memset(buffer, 0, 17);

        sendServerCommand(SERVER_SEND_16);
        sleep(1);

        size_t offset = 0;
        size_t count = 16;
        while (count > 0) {
            size_t return_status = inetPeer->receive(&buffer[offset], count);
            CPPUNIT_ASSERT(return_status > 0);
            offset += return_status;
            count -= return_status;
        }
        CPPUNIT_ASSERT(strcmp(buffer,STANDARD_STRING16) == 0);

        sendServerCommand(SERVER_SEND_16);
        sleep(1);

        memset(buffer, 0, 17);
        CPPUNIT_ASSERT(inetPeer->receive_all(buffer,16));
        CPPUNIT_ASSERT(strcmp(buffer,STANDARD_STRING16) == 0);

        delete inetPeer;
    }

    //
    // cause a SIGALRM interrupt to happen while doing a receive
    // bogus
    //
    {
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(inetPeer != nullptr);

        char buffer[17];
        memset(buffer, 0, 17);

        sendServerCommand(SERVER_SEND_16_AFTER_10_SECONDS);
        sleep(1);

        size_t offset = 0;
        size_t count = 16;
        alarm(2);
        while (count > 0) {
            size_t return_status = inetPeer->receive(&buffer[offset], count);
            CPPUNIT_ASSERT(return_status > 0);
            offset += return_status;
            count -= return_status;
        }
        CPPUNIT_ASSERT(strcmp(buffer,STANDARD_STRING16) == 0);

        sendServerCommand(SERVER_SEND_16_AFTER_10_SECONDS);
        sleep(1);

        memset(buffer, 0, 17);
        alarm(2);
        CPPUNIT_ASSERT(inetPeer->receive_all(buffer,16));
        CPPUNIT_ASSERT(strcmp(buffer,STANDARD_STRING16) == 0);

        delete inetPeer;
    }


    stopInetServer();
    TRACE;
}

void TestPeerClasses::testSocketPeerSend()
{
    InetSocketPeer* inetPeer = nullptr;

    TRACE;
    //
    // try sending to an uninitialized InetSocketPeer
    //
    {
        inetPeer = new InetSocketPeer();
        char buffer[17]="0123456789012345";
        try {
            inetPeer->send(buffer,16);
            CPPUNIT_ASSERT(false && "didn't get exception for send on uninitialized InetSocketPeer");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EBADF);
            CPPUNIT_ASSERT(e.code() == PeerException::INVALID_DESCRIPTOR);
        }
        delete inetPeer;
    }

    TRACE;
    //
    // try sending on a listening socket
    //
    {
        char buffer[17]="0123456789012345";
        inetPeer = new InetSocketPeer();
        inetPeer->listen(STANDARD_PORT);
        try {
            inetPeer->send(buffer, 16);
            CPPUNIT_ASSERT(false && "didn't get exception for send on a listening InetSocketPeer");
        } catch (InvalidParameterError&) {
        }
        delete inetPeer;
    }

    TRACE;
    startInetServer();

    TRACE;
    //
    // Do some checking on parameter validity
    //
    {
        char buffer[17]="0123456789012345";
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(inetPeer != nullptr);

        //
        // send 0 bytes with a valid pointer
        //
        CPPUNIT_ASSERT(inetPeer->send(buffer, 0));

        //
        // send 0 bytes with a null pointer
        //
        CPPUNIT_ASSERT(inetPeer->send(nullptr, 0));

        //
        // send 16 bytes with null pointer
        //
        try {
           CPPUNIT_ASSERT(inetPeer->send(nullptr, 16));
           CPPUNIT_ASSERT(false && "didn't get exception for null pointer in send");
        } catch (InvalidParameterError&) { 
        }

        delete inetPeer;
    }

    TRACE;
    //
    // do a basic functioning send
    //
    {
        char buffer[17]="0123456789012345";
        inetPeer = new InetSocketPeer("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(inetPeer != nullptr);

        CPPUNIT_ASSERT(inetPeer->send(buffer,16));

        delete inetPeer;
    }

    stopInetServer();
}


void TestPeerClasses::testSocketPeerConstructAndPoll()
{

    SocketPeer* peer = nullptr;
    InetSocketPeer* inetPeer = nullptr;
    CPPUNIT_ASSERT(inetPeer == nullptr);  // keep compiler happy
    CPPUNIT_ASSERT(peer == nullptr);  // keep compiler happy
    TRACE;
    //
    // try to create a socket with a bad file descriptor
    //
    {
        try {
            peer = new SocketPeer(100);
            CPPUNIT_ASSERT(false && "didn't get exception for use of bad port");
        } catch (InvalidParameterError&) {
        }
    }

  
    TRACE;
    //
    // try using a non-socket file descriptor
    //
    {
        int infile = open("/dev/null", O_RDONLY);
        CPPUNIT_ASSERT(infile >= 0);
        try {
            peer = new SocketPeer(infile);
            CPPUNIT_ASSERT(false && "didn't get exception for use of non-socket");
        } catch (InvalidParameterError&) {
        }
        CPPUNIT_ASSERT(close(infile) == 0);
    }

    //
    // testing poll
    //

    TRACE;
    //
    // try to do a poll on peer with no assigned socket
    //
    {
        inetPeer = new InetSocketPeer();
        CPPUNIT_ASSERT(inetPeer != nullptr);

        try {
            bool read_status;
            bool write_status;
            inetPeer->poll(true, read_status, true, write_status, 100);
            CPPUNIT_ASSERT(false && "didn't get exception for poll of peer with no assigned socket");
        } catch (InvalidParameterError&) {
        }
        delete inetPeer;
    }

    TRACE;
    //
    // try to do a poll on a listening socket
    //
    {
        inetPeer = new InetSocketPeer();
        CPPUNIT_ASSERT(inetPeer != nullptr);
        inetPeer->listen(STANDARD_PORT);
        try {
            bool read_status;
            bool write_status;
            inetPeer->poll(true, read_status, true, write_status, 100);
            CPPUNIT_ASSERT(false && "didn't get exception for poll on listening socket");
        } catch (InvalidParameterError&) {
        }
        delete inetPeer;
    }


    TRACE;
    startInetServer();

    TRACE;
    //
    // create a connection for use with multiple tests
    //

    inetPeer = new InetSocketPeer();
    inetPeer->connect("localhost", STANDARD_PORT);

    TRACE;
    // try to do a poll when not interested in either read or write
    {
        try {
            bool read_status;
            bool write_status;
            inetPeer->poll(false, read_status, false, write_status, 100);
            CPPUNIT_ASSERT(false && "didn't get exception for poll on listening socket");
        } catch (InvalidParameterError&) {
        }
    }

    TRACE;
    //
    // do a poll for writes being possible. It should normally return immediately
    // shouldn't matter if reads are also selected
    //
    {
        bool read_status = true;
        bool write_status = false;
        TestTimer timer;
        timer.start();
        bool return_status = inetPeer->poll(false, read_status, true, write_status, 10000);
        timer.stop();
        CPPUNIT_ASSERT(timer.timeElapsed() < 0.1);
        CPPUNIT_ASSERT(read_status); // read status shouldn't have changed
        CPPUNIT_ASSERT(write_status);
        CPPUNIT_ASSERT(!return_status);
    }

    TRACE;
    //
    // do a poll of either reads or writes being possible. Since writes will be possible
    // it should still return immediately.
    //
    {
        bool read_status = true;
        bool write_status = false;
        TestTimer timer;
        timer.start();
        bool return_status = inetPeer->poll(true, read_status, true, write_status, 10000);
        timer.stop();
        CPPUNIT_ASSERT(timer.timeElapsed() < 0.1);
        CPPUNIT_ASSERT(!read_status);
        CPPUNIT_ASSERT(write_status);
        CPPUNIT_ASSERT(!return_status);
    }

    TRACE;
    //
    // do a poll for reads being possible with the default timeout of 0. Should return immediately
    //
    {
        bool read_status = true;
        bool write_status = true;
        TestTimer timer;
        timer.start();
        bool return_status = inetPeer->poll(true, read_status, false, write_status);
        timer.stop();
        CPPUNIT_ASSERT(!read_status);
        CPPUNIT_ASSERT(write_status); // write shouldn't have changed
        CPPUNIT_ASSERT(!return_status);
        CPPUNIT_ASSERT(timer.timeElapsed() < 0.1);
    }

    TRACE;
    //
    // do it again with the write_status preset to false to make sure it isn't being set
    //
    {
        bool read_status = true;
        bool write_status = false;
        TestTimer timer;
        timer.start();
        bool return_status = inetPeer->poll(true, read_status, false, write_status);
        timer.stop();
        CPPUNIT_ASSERT(!read_status);
        CPPUNIT_ASSERT(!write_status); // write shouldn't have changed
        CPPUNIT_ASSERT(!return_status);
        CPPUNIT_ASSERT(timer.timeElapsed() < 0.1);
    }


    TRACE;
    //
    // do a poll for reads being possible with the expectation of just waiting for timeout
    //
    {
        bool read_status = true;
        bool write_status = true;
        TestTimer timer;
        timer.start();
        bool return_status = inetPeer->poll(true, read_status, false, write_status, 100);
        timer.stop();
        CPPUNIT_ASSERT(!read_status);
        CPPUNIT_ASSERT(write_status); // write shouldn't have changed
        CPPUNIT_ASSERT(!return_status);
        CPPUNIT_ASSERT(timer.timeElapsed() < 0.2);
        CPPUNIT_ASSERT(timer.timeElapsed() > 0);
    }

    TRACE;
    //
    // do a poll where a SIGALRM happens in the middle of it
    //
    {
        bool read_status = true;
        bool write_status = true;
        TestTimer timer;
        alarm(2);
        timer.start();
        bool return_status = inetPeer->poll(true, read_status, false, write_status, 100);
        timer.stop();
        CPPUNIT_ASSERT(!read_status);
        CPPUNIT_ASSERT(write_status); // write shouldn't have changed
        CPPUNIT_ASSERT(!return_status);
        CPPUNIT_ASSERT(timer.timeElapsed() < 0.2);
        CPPUNIT_ASSERT(timer.timeElapsed() > 0);
    }

    hangServer=false;
    delete inetPeer;
    stopInetServer();

}

void TestPeerClasses::testInetSocketPeer()
{
    InetSocketPeer* peer;

    TRACE;
    //
    // try listening to a low numbered port
    //
    {
        peer = new InetSocketPeer();

        try {
            peer->listen(22); // 22 is used by ssh
            CPPUNIT_ASSERT(false && "didn't get exception for attempting to use low port");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EACCES);
            CPPUNIT_ASSERT(e.code() == PeerException::PERMISSION_DENIED);
        }

        delete peer;
    }

    TRACE;
    //
    // use 0 as the port to let a port be assigned
    //
    { 
        peer = new InetSocketPeer();

        peer->listen(0);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        unsigned short port = peer->localPort();
        CPPUNIT_ASSERT(port > 0);

        //
        // listen to a port which is already in use
        //
        InetSocketPeer peer2;
        try {
            // try listening to a port which is already in use
            peer2.listen(port);
            CPPUNIT_ASSERT(false && "didn't get exception for attempting to use busy port");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EADDRINUSE);
            CPPUNIT_ASSERT(e.code() == PeerException::IN_USE);
        }

        //
        // attempt to listen using a InetSocketPeer which already has a socket assigned
        try {
            peer->listen(0);
            CPPUNIT_ASSERT(false && "didn't get exception when socket is already assigned");
	} catch (InvalidParameterError&) {
        }

        delete peer;
    }

    TRACE;
    //
    // test for failure due to exhausted file handles
    //
    {
        peer = new InetSocketPeer();

        struct rlimit oldlimit;
        struct rlimit newlimit;
        CPPUNIT_ASSERT(getrlimit(RLIMIT_NOFILE, &oldlimit) == 0);
        newlimit.rlim_max = oldlimit.rlim_max;
        newlimit.rlim_cur = 10;
        setrlimit(RLIMIT_NOFILE, &newlimit);

        // burn file handles
        int handles[20];
        int index=0;
        for (index=0; index < 20; index++) {
            handles[index]= open("/dev/null", O_RDONLY);
            if (handles[index] < 0) break;
        }
   
        try {
            peer->listen(0);
            CPPUNIT_ASSERT(false && "didn't get exception for listening to a Inet socket when the file handles are exhausted");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EMFILE);
            CPPUNIT_ASSERT(e.code() == PeerException::FILES);
        }

        // free up the file handles
        for (int i = 0; i < index; i++) {
            CPPUNIT_ASSERT(close(handles[i]) == 0);
        }
        // put the file handle limit back the way it was
        setrlimit(RLIMIT_NOFILE, &oldlimit);

        delete peer;
    }

    TRACE;
    //
    // listen on a specific port number
    //
    {
        peer = new InetSocketPeer();
        peer->listen(STANDARD_PORT);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        CPPUNIT_ASSERT(peer->localPort() == STANDARD_PORT);
        delete peer;
    }

    TRACE;
    //
    // try the constructor which wraps listen
    //
    {
        peer = new InetSocketPeer(STANDARD_PORT);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        CPPUNIT_ASSERT(peer->localPort() == STANDARD_PORT);
        delete peer;
    }

    TRACE;
    //
    //
    // Test InetSocketPeer connect
    //
    //
    {
        peer = new InetSocketPeer();

        // attempt to connect to a non-existent system
        try {
            peer->connect("aljksdfasdfasdasdf", STANDARD_PORT);
            CPPUNIT_ASSERT(false && "didn't get exception for connect to invalid system name");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == 0);
            CPPUNIT_ASSERT(e.code() == PeerException::NO_HOST);
        }

        // attempt to connect to an empty string
        try {
            peer->connect(std::string(), STANDARD_PORT);
            CPPUNIT_ASSERT(false && "didn't get exception for connect to empty system name");
        } catch (InvalidParameterError&) {
        }

        // attempt to connect to zero length string
        try {
            peer->connect("", STANDARD_PORT);
            CPPUNIT_ASSERT(false && "didn't get exception for connect to zero length system name");
        } catch (InvalidParameterError&) {
        }

        // attempt to connect to a zero port
        try {
            peer->connect("localhost", 0);
            CPPUNIT_ASSERT(false && "didn't get exception for connect to zero port");
        } catch (InvalidParameterError&) {
        }

        delete peer;
    }

    TRACE;
    //
    // test for failure due to exhausted file handles
    //
    {
        peer = new InetSocketPeer();

        struct rlimit oldlimit;
        struct rlimit newlimit;
        CPPUNIT_ASSERT(getrlimit(RLIMIT_NOFILE, &oldlimit) == 0);
        newlimit.rlim_max = oldlimit.rlim_max;
        newlimit.rlim_cur = 10;
        setrlimit(RLIMIT_NOFILE, &newlimit);

        // burn file handles
        int handles[20];
        int index=0;
        for (index=0; index < 20; index++) {
            handles[index]= open("/dev/null", O_RDONLY);
            if (handles[index] < 0) break;
        }
   
        try {
            peer->connect("localhost", STANDARD_PORT);
            CPPUNIT_ASSERT(false && "didn't get exception for connect when the file handles are exhausted");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EMFILE);
            CPPUNIT_ASSERT(e.code() == PeerException::FILES);
        }

        // free up the file handles
        for (int i = 0; i < index; i++) {
            CPPUNIT_ASSERT(close(handles[i]) == 0);
        }
        // put the file handle limit back the way it was
        setrlimit(RLIMIT_NOFILE, &oldlimit);

        delete peer;
    }

    TRACE;
    startInetServer();

    //
    // try successful connection
    //
    {
        peer = new InetSocketPeer();
        peer->connect("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        delete peer;
    }

    TRACE;
    //
    // try successful connection using constructor wrapper
    //
    {
        peer = new InetSocketPeer("localhost", STANDARD_PORT);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        delete peer;
    }

    TRACE;
    stopInetServer();
    TRACE;
}

bool exists(const char* path)
{
    return (access(path, F_OK) == 0);
}

void TestPeerClasses::testIPCSocketPeer()
{
    IPCSocketPeer* peer = new IPCSocketPeer();
    CPPUNIT_ASSERT( peer != nullptr);

    // remove the bind point in case it's left over from a previous run
    unlink(STANDARD_NAME);

    TRACE;
    //
    //
    // Testing of IPCSocketPeer::listen
    //
    //
    // do a simple listen that should succeed
    peer->listen(STANDARD_NAME);
    CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
    // make sure the bind point exists
    CPPUNIT_ASSERT(exists(STANDARD_NAME));
    delete peer;
    // make sure the bind point doesn't exist (destroyed by destructor)
    CPPUNIT_ASSERT(!exists(STANDARD_NAME));
    CPPUNIT_ASSERT(errno == ENOENT);

    {
        peer = new IPCSocketPeer();

        //
        // use an invalid 0 aMaxPendingConnections
        //
        try {
            peer->listen(STANDARD_NAME, 0);
            CPPUNIT_ASSERT(false && "didn't get exception for 0 max pending connections");
        } catch (InvalidParameterError&) {
        }

        //
        // use an invalid negative aMaxPendingConnections
        //
        try {
            peer->listen(STANDARD_NAME, -5);
            CPPUNIT_ASSERT(false && "didn't get exception for -5 max pending connections");
        } catch (InvalidParameterError&) {
        }

        //
        // try an invalid empty name
        //
        try {
            peer->listen(std::string());
            CPPUNIT_ASSERT(false && "didn't get exception for empty IPC name");
        } catch (InvalidParameterError&) {
        }

        //
        // try an invalid empty name
        //
        try {
            peer->listen("");
            CPPUNIT_ASSERT(false && "didn't get exception for zero length IPC name");
        } catch (InvalidParameterError&) {
        }

        //
        // try a name which is too long
        //
        try {
            peer->listen("/tmp/check_that_a_108_character_name_is_not_accepted_as_an_IPC_name_since_that_is_too_long_for___an_IPC_name");
            CPPUNIT_ASSERT(false && "didn't get exception for IPC name that is too long");
        } catch (InvalidParameterError&) {
        }

        delete peer;
    }

    TRACE;
    //
    // test some permission and file type errors
    //
    {
        peer = new IPCSocketPeer();

        // get rid of existing files
        chmod("/tmp/bad_permission_dir", 0777);
        unlink("/tmp/bad_permission_dir/ipcname");
        rmdir("/tmp/bad_permission_dir");

        CPPUNIT_ASSERT(mkdir("/tmp/bad_permission_dir", 0777) == 0);
        // make it so the file file can't be unlinked
        chmod("/tmp/bad_permission_dir", 0444);

        //
        // test for use of an ipc name with permission issues
        //
        try {
            peer->listen("/tmp/bad_permission_dir/ipcname");
            CPPUNIT_ASSERT(false && "didn't get exception for inability to unlink the file");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EACCES);
            CPPUNIT_ASSERT(e.code() == PeerException::PERMISSION_DENIED);
        }

        //
        // test for use of an ipc name with a non directory used as a directory
        //
        CPPUNIT_ASSERT(chmod("/tmp/bad_permission_dir", 0777) == 0);
        int file = open("/tmp/bad_permission_dir/ipcname", O_CREAT, 777);
        CPPUNIT_ASSERT(file >= 0);
        CPPUNIT_ASSERT(close(file) == 0);
        try {
            peer->listen("/tmp/bad_permission_dir/ipcname/ipcname");
            CPPUNIT_ASSERT(false && "didn't get exception for path that uses regular file as directory");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == ENOTDIR);
            CPPUNIT_ASSERT(e.code() == PeerException::FILES);
        }

        // clean up the file
        CPPUNIT_ASSERT(chmod("/tmp/bad_permission_dir", 0777) == 0);
        CPPUNIT_ASSERT(unlink("/tmp/bad_permission_dir/ipcname") == 0);
        CPPUNIT_ASSERT(rmdir("/tmp/bad_permission_dir") == 0);

        delete peer;
    }

    TRACE;

    //
    // test for failure due to exhausted file handles
    //
    {
        peer = new IPCSocketPeer();

        struct rlimit oldlimit;
        struct rlimit newlimit;
        CPPUNIT_ASSERT(getrlimit(RLIMIT_NOFILE, &oldlimit) == 0);
        newlimit.rlim_max = oldlimit.rlim_max;
        newlimit.rlim_cur = 10;
        setrlimit(RLIMIT_NOFILE, &newlimit);

        // burn file handles
        int handles[20];
        int index=0;
        for (index=0; index < 20; index++) {
            handles[index]= open("/dev/null", O_RDONLY);
            if (handles[index] < 0) break;
        }
    
        try {
            peer->listen("/tmp/test_peer");
            CPPUNIT_ASSERT(false && "didn't get exception create a socket when the file handles are exhausted");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EMFILE);
            CPPUNIT_ASSERT(e.code() == PeerException::FILES);
        }

        // free up the file handles
        for (int i = 0; i < index; i++) {
            CPPUNIT_ASSERT(close(handles[i]) == 0);
        }
        // put the file handle limit back the way it was
        setrlimit(RLIMIT_NOFILE, &oldlimit);

        delete peer;
    }

    TRACE;
    //
    // check for error on more than one listener on the same ipc name 
    //
    {
        peer = new IPCSocketPeer();

        // remove the name in case this test aborted before
        unlink(STANDARD_NAME);

        peer->listen(STANDARD_NAME);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);

        IPCSocketPeer peer2;
        try {
            peer2.listen(STANDARD_NAME);
            CPPUNIT_ASSERT(false && "didn't get exception for multiple listeners on same IPC name");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EADDRINUSE);
            CPPUNIT_ASSERT(e.code() == PeerException::IN_USE);
        }

        // clean up
        delete peer;
    }

    TRACE;
    //
    // test with the maximum name length
    //
    {
        peer = new IPCSocketPeer();

        unlink(LONG_NAME_107);
        peer->listen(LONG_NAME_107);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        // need to clean up after a success
        delete peer;
    }

    TRACE;
    //
    // make sure there isn't an undocumented maximum to max pending connections
    //
    {
        peer = new IPCSocketPeer();

        peer->listen(STANDARD_NAME, std::numeric_limits<int>::max());
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);

        delete peer;
    }

    TRACE;
    //
    // test trying to listen using an IPCSocketPeer which already has a socket
    //
    {
        peer = new IPCSocketPeer();
        peer->listen(STANDARD_NAME);
        try {
            peer->listen(STANDARD_NAME);
            CPPUNIT_ASSERT(false && "didn't get exception for using an IPCSocketPeer which already has a socket");
        } catch (InvalidParameterError&) {
        }
        delete peer;
    }

    TRACE;
    //
    //
    // testing of IPCSocketPeer::connect()
    //
    //

    startIPCServer(STANDARD_NAME);

    TRACE;
    //
    // do a simple connect that should succeed
    //
    {
        peer = new IPCSocketPeer();

        peer->connect(STANDARD_NAME);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        delete peer;
    }

    TRACE;
    {
        peer = new IPCSocketPeer();

        //
        // test zero length string
        //
        try {
            peer->connect("");
            CPPUNIT_ASSERT(false && "didn't get exception for connect on an zero length string");
        } catch (InvalidParameterError&) {
        }

        //
        // test empty string
        //
        try {
            peer->connect(std::string());
            CPPUNIT_ASSERT(false && "didn't get exception for connect on an empty string");
        } catch (InvalidParameterError&) {
        }

        //
        // test name which is too long
        //
        try {
            peer->connect("/tmp/check_that_a_108_character_name_is_not_accepted_as_an_IPC_name_since_that_is_too_long_for___an_IPC_name");
            CPPUNIT_ASSERT(false && "didn't get exception for connect on an name which is too long");
        } catch (InvalidParameterError&) {
        }

        delete peer;
    }


    TRACE;
    //
    // test for connect failure due to exhausted file handles
    //
    {
        peer = new IPCSocketPeer();

        struct rlimit oldlimit;
        struct rlimit newlimit;
        CPPUNIT_ASSERT(getrlimit(RLIMIT_NOFILE, &oldlimit) == 0);
        newlimit.rlim_max = oldlimit.rlim_max;
        newlimit.rlim_cur = 10;
        setrlimit(RLIMIT_NOFILE, &newlimit);

        // burn file handles
        int handles[20];
        int index=0;
        for (index=0; index < 20; index++) {
            handles[index]= open("/dev/null", O_RDONLY);
            if (handles[index] < 0) break;
        }

        try {
            peer->connect(STANDARD_NAME);
            CPPUNIT_ASSERT(false && "didn't get exception create a socket when the file handles are exhausted");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EMFILE);
            CPPUNIT_ASSERT(e.code() == PeerException::FILES);
        }

        // free up the file handles
        for (int i = 0; i < index; i++) {
            CPPUNIT_ASSERT(close(handles[i]) == 0);
        }
        // put the file handle limit back the way it was
        setrlimit(RLIMIT_NOFILE, &oldlimit);

        delete peer;
    }

    TRACE;
    //
    // test trying to connect using an IPCSocketPeer which already has a socket
    //
    {
        peer = new IPCSocketPeer();
        peer->connect(STANDARD_NAME);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        try {
            peer->connect(STANDARD_NAME);
            CPPUNIT_ASSERT(false && "didn't get exception for connect using an IPCSocketPeer which already has a socket");
        } catch (InvalidParameterError&) {
        }
        delete peer;
    }

    TRACE;
    //
    // try connecting to a name where a listener wasn't set up
    //
    {
        peer = new IPCSocketPeer();

        unlink("/tmp/nothing_is_listening");
        try {
            peer->connect("/tmp/nothing_is_listening");
            CPPUNIT_ASSERT(false && "didn't get exception for no ipc name");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == ENOENT);
            CPPUNIT_ASSERT(e.code() == PeerException::FILES);
        }

        delete peer;
    }

    TRACE;
    //
    // try connecting to a name where the file exists but it has no bound listener
    //
    {
        peer = new IPCSocketPeer();

        int file = open("/tmp/nothing_is_listening", O_CREAT, 0777);
        CPPUNIT_ASSERT(file >= 0);
        CPPUNIT_ASSERT(close(file) == 0);
        try {
            peer->connect("/tmp/nothing_is_listening");
            CPPUNIT_ASSERT(false && "didn't get exception for no listener");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == ECONNREFUSED);
            CPPUNIT_ASSERT(e.code() == PeerException::CONNECTION_REFUSED);
        }
        unlink("/tmp/nothing_is_listening");

        delete peer;
    }


    TRACE;
    //
    // try connecting when there will be no permission to connect
    // remove permission
    //
    {
        peer = new IPCSocketPeer();
        chmod(STANDARD_NAME, 0);
        try {
            peer->connect(STANDARD_NAME);
            CPPUNIT_ASSERT(false && "didn't get exception for connect without permissions");
        } catch (PeerException& e) {
            CPPUNIT_ASSERT(e.error() == EACCES);
            CPPUNIT_ASSERT(e.code() == PeerException::PERMISSION_DENIED);
        }
        // restore permission
        chmod(STANDARD_NAME, 0775);

        delete peer;
    }


    TRACE;
    //
    // try connecting to a 107 character name
    //
    // need to shut down server and start a new one listening to new name
    stopIPCServer();
    startIPCServer(LONG_NAME_107);

    {

        peer = new IPCSocketPeer();
        peer->connect(serverIPCName);
        CPPUNIT_ASSERT(peer->fd() != ARRAS_INVALID_SOCKET);
        delete peer;
    }

    stopIPCServer();
}
 
void sigalrm_handler(int /*signum*/)
{
}

void TestPeerClasses::testPeerClasses()
{
    struct sigaction alrmaction;
    struct sigaction oldaction;
    memset(&alrmaction, 0, sizeof(struct sigaction));
    memset(&oldaction, 0, sizeof(struct sigaction));
    alrmaction.sa_handler = sigalrm_handler;
    sigemptyset(&alrmaction.sa_mask);
    sigaction(SIGALRM, NULL, &oldaction);
    sigaction(SIGALRM, &alrmaction, NULL);
    
    TRACE;
    testIPCSocketPeer();
    TRACE;
    testInetSocketPeer();
    TRACE;
    testSocketPeerConstructAndPoll();
    TRACE;
    testSocketPeerSend();
    TRACE;
    testSocketPeerReceive();
    TRACE;
    testSocketPeerPeek();
    TRACE;
    testSocketPeerAccept();
    TRACE;
}

