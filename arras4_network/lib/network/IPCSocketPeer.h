// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_IPCSOCKETPEER_H__
#define __ARRAS4_IPCSOCKETPEER_H__

#include "SocketPeer.h"

namespace arras4 {
namespace network {

class IPCSocketPeer : public SocketPeer {
  public:
    IPCSocketPeer();
    ~IPCSocketPeer();
    void listen(const std::string& aIPCName, int aMaxPendingConnections = 32);
    void connect(const std::string& aIPCName);

  private:
    std::string mSocketName;
};

} // end namespace network
} // end namespace arras

#endif // __ARRAS4_IPCSOCKETPEER_H__

