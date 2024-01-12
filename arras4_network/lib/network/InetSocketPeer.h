// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_INETSOCKETPEER_H__
#define __ARRAS4_INETSOCKETPEER_H__

#include "SocketPeer.h"

namespace arras4 {
namespace network {

class InetSocketPeer : public SocketPeer 
{
  public:
    InetSocketPeer();
    InetSocketPeer(unsigned short aPort, int aMaxPendingConnections = 32);
    InetSocketPeer(const std::string& aHostname, unsigned short aPort);
    ~InetSocketPeer();
    void listen(unsigned short aPort, int aMaxPendingConnections = 32);
    void connect(const std::string& aHostname, unsigned short aPort);
    unsigned short localPort() const {
        return mLocalPort;
    }

  private:
    // these two are for UDP socket connections, to identify the default
    // endpoint that we connected to originally (only used for clients)
    unsigned int mIPv4;
    unsigned short mPort;
    unsigned short mLocalPort;
};

} // end namespace network
} // end namespace arras

#endif // __ARRAS_INETSOCKETPEER_H__

