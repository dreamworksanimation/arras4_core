// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "UdpSyslog.h"

namespace arras4 {
namespace log {

const int MAX_SENDTO_RETRIES = 5;

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

    // send_to can on rare occassons throw system_error because the underlying
    // system call sendto() can randomly return EPERM. Just retry and in the
    // extremely unlikely case of not working on retries then just drop it
    for (int i = 0; i < MAX_SENDTO_RETRIES; i++) {
        try {
            mSocket.send_to(boost::asio::buffer(packet.data(), packet.size()), mTarget);

            // if it succeeds then break out of the loop
            break; 
        } catch (const boost::system::system_error& e) {
            if (e.code() != boost::asio::error::basic_errors::no_permission) {
                throw;
            }
        }
    }
}

}
}
