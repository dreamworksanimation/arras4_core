// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_UDP_SYSLOG_H__
#define __ARRAS4_UDP_SYSLOG_H__

#include <time.h>
#include <sstream>
#include <iomanip>

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/host_name.hpp>

// warning : don't include <syslog.h> here : it makes
// some of the Logger::Level enum values unusable...

namespace arras4 {
namespace log {

// Sends log messages to syslog service (typically port 514) using
// UDP
class UdpSyslog
{
    public:

    // Will send messages to given host name (e.g. localhost) and port (e.g. 514)
    UdpSyslog(const std::string& addr, unsigned short port)
        : mSocket(mService)
     {
        // bind socket to any local address 
        mSocket.open(boost::asio::ip::udp::v4());
        mSocket.set_option(boost::asio::socket_base::reuse_address(true));
        boost::asio::ip::udp::endpoint anyAddress;
        mSocket.bind(anyAddress);

        // resolve hostname to target endpoint
        boost::asio::ip::udp::resolver hostNameResolver(mService);
        boost::asio::ip::udp::resolver::query q(boost::asio::ip::udp::v4(),
                                                addr,
                                                std::to_string(port), 
                                                boost::asio::ip::resolver_query_base::address_configured);
        mTarget = *hostNameResolver.resolve(q);

        // get local host name
        boost::system::error_code err;
        mLocalHostName = boost::asio::ip::host_name(err);
    }

    ~UdpSyslog() 
    {
         boost::system::error_code err;
         mSocket.shutdown(boost::asio::socket_base::shutdown_both, err);
         mSocket.close(err);
    }

    // send message with given priority (e.g. LOG_LOCAL0 | LOG_DEBUG), time, ident and message 
    void sendMessage(int priority,const tm* timeStamp, const std::string& ident, const std::string& message);
      
private:

    boost::asio::io_service mService;
    boost::asio::ip::udp::socket mSocket;
    boost::asio::ip::udp::endpoint mTarget;
    std::string mLocalHostName;
};

}
}
#endif
