// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_DATAOUTSTREAM_H__
#define __ARRAS_DATAOUTSTREAM_H__

#include "messageapi_types.h"
#include <cstdint>
#include <string>

namespace arras4
{
    namespace api {

// DataOutStream is the interface supplied to the serialize function
// of an ObjectContent subclass. The 'write' functions are provided for
// compatibility with Arras 3, and in case the writer needs to know
// how many bytes were written. Stream operators are also available, and
// are recommended for easy-to-read code.
class DataOutStream 
{
public:
    virtual ~DataOutStream() {}
    virtual size_t write(const void* aBuf, size_t aLen)=0;
    virtual size_t fill(unsigned char aByte,size_t aCount)=0;
    virtual void flush()=0;

    // total bytes written on this stream
    virtual size_t bytesWritten() const=0;

    // write specific types. The encoding protocol for these
    // types is encapsulated in this class (and DataInStream)
    virtual size_t write(bool val)=0;
    virtual size_t write(int8_t val)=0;
    virtual size_t write(int16_t val)=0;
    virtual size_t write(int32_t val)=0;
    virtual size_t write(int64_t val)=0;
    virtual size_t write(uint8_t val)=0;
    virtual size_t write(uint16_t val)=0;
    virtual size_t write(uint32_t val)=0;
    virtual size_t write(uint64_t val)=0;
    virtual size_t write(float aFloat)=0;
    virtual size_t write(double aDouble)=0;
    virtual size_t write(const std::string& aString)=0;                // string length is UINT32
    virtual size_t writeLongString(const std::string& aString)=0;      // string length is size_t
    virtual size_t write(const UUID& uuid)=0;
    virtual size_t write(const ArrasTime& time)=0;
    virtual size_t write(const Address& address)=0;
};

// alternate interface using << operator
template <typename T> DataOutStream& operator<<(DataOutStream& os,T val)                 
{ 
    os.write(val); 
    return os; 
}

} 
} 
#endif
