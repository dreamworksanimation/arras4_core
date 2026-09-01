// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "UdpSyslog_boost.h"

namespace arras4 {
namespace log {

void UdpSyslog::sendMessage(int priority, 
                            const tm* timeStamp,
                            const std::string& ident, 
                            const std::string& message)
{


    static const char months[12][4] =
        {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };

    // format from RFC 3164 : BSD syslog Protocol
    std::stringstream ss;
    ss << "<" << priority << + ">"
       << months[timeStamp->tm_mon] << " "
       << std::setfill(' ')
       << std::setw(2) << timeStamp->tm_mday << " "
       << std::setfill('0')
       << std::setw(2) << timeStamp->tm_hour << ":"
       << std::setw(2) << timeStamp->tm_min << ":"
       << std::setw(2) << timeStamp->tm_sec << " "
       << mLocalHostName << " "
       << ident << " ";

    // truncate super long messages
    // There message size limit for datagrams is 64K (65507 chars without packet header)
    // Using 65000 to allow room for log header
    size_t size = message.size();
    if (size > 65000) {
       ss << message.substr(0,65000) << " (Truncated from " << size << " chars)";
    } else {
       ss << message;
    }
   
    std::string packet = ss.str();

    // The socket is non-blocking because this path is called inline from Arras
    // message delivery. Drop telemetry when the UDP target is unavailable or
    // its send buffer is full; logging must not delay or abort rendering.
    boost::system::error_code error;
    mSocket.send_to(boost::asio::buffer(packet.data(), packet.size()),
                    mTarget,
                    0,
                    error);
}

}
}
