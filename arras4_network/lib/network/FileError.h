// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_FILE_ERROR_H__
#define __ARRAS4_FILE_ERROR_H__

#include <exception>
#include <string>

namespace arras4 {
    namespace network {

class FileError : public std::exception
{
public:
   
    FileError(const std::string& aMsg)
        : mMsg(aMsg) {}
   

    ~FileError() throw() {}
    const char* what() const throw() { return mMsg.c_str(); }
   
protected:
    std::string mMsg;
};

}
}
#endif
