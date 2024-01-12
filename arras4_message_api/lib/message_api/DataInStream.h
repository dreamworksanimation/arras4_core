// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_DATAINSTREAM_H__
#define __ARRAS_DATAINSTREAM_H__

#include "messageapi_types.h"
#include <cstdint>
#include <string>

namespace arras4
{
    namespace api {

// DataInStream is the interface supplied to the deserialize function
// of an ObjectContent subclass. The 'read' functions are provided for
// compatibility with Arras 3, and in case the reader needs to know
// how many bytes were read. Stream operators are also available, and
// are recommended for easy-to-read code.
class DataInStream
{
public:
    virtual ~DataInStream() {}
    virtual size_t read(void* aBuf, size_t aLen)=0;
    virtual size_t skip(size_t aLen)=0; 
    virtual size_t bytesRead() const=0;

    virtual size_t read(bool& val)=0;
    virtual size_t read(int8_t& val)=0;
    virtual size_t read(int16_t& val)=0;
    virtual size_t read(int32_t& val)=0;
    virtual size_t read(int64_t& val)=0;
    virtual size_t read(uint8_t& val)=0;
    virtual size_t read(uint16_t& val)=0;
    virtual size_t read(uint32_t& val)=0;
    virtual size_t read(uint64_t& val)=0;
    virtual size_t read(float& val)=0;
    virtual size_t read(double& val)=0;
    virtual size_t read(std::string& val)=0;            // string length is uint32_t
    virtual size_t readLongString(std::string& val)=0;  // string length is size_t
    virtual size_t read(UUID& uuid)=0;
    virtual size_t read(ArrasTime& time)=0;
    virtual size_t read(Address& address)=0;
};
    
// alternate interface using >> operator
template <typename T> DataInStream& operator>>(DataInStream& is, T& val)                 
{ 
    is.read(val); 
    return is; 
}

} 
} 

#endif // __ARRAS_DATAINSTREAM_H__

