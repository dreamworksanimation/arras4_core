// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_FILE_DATA_SOURCEH__
#define __ARRAS4_FILE_DATA_SOURCEH__

#include "DataSource.h"

#include <fstream>

namespace arras4 {
    namespace network {

class FileDataSource : public DataSource
{
public:
    FileDataSource(const std::string& filepath);
    ~FileDataSource();
   
    size_t read(unsigned char* aBuf, size_t aLen);
    size_t skip(size_t aLen);
    size_t bytesRead() const;
   
private:
    std::string mFilepath;
    std::ifstream mStream;
};

}
}
#endif
  

