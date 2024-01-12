// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "FileDataSource.h"
#include "FileError.h"

namespace arras4 {
    namespace network {

FileDataSource::FileDataSource(const std::string& filepath)
    : mFilepath(filepath),
      mStream(filepath)
{
    if (!mStream)
        throw FileError("failed to open file " + filepath);
}

FileDataSource::~FileDataSource()
{
}
   
size_t FileDataSource::read(unsigned char* aBuf, size_t aLen)
{
    if (!mStream)
        throw FileError("failed to read file " + mFilepath);
    mStream.read(reinterpret_cast<char*>(aBuf), aLen);
    return mStream.gcount();
}

size_t FileDataSource::skip(size_t aLen)
{
    if (!mStream)
        throw FileError("failed to read file " + mFilepath);
    mStream.ignore(aLen);
    return mStream.gcount();
}

size_t FileDataSource::bytesRead() const
{
    std::streampos s = const_cast<std::ifstream&>(mStream).tellg();
    if (s == -1)
        throw FileError("failed to get file read position: " + mFilepath);
    return s;
}

}
}
