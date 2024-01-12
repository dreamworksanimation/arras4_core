// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "StreamImpl.h"

#include <network/DataSource.h>
#include <network/DataSink.h>

#include <message_api/UUID.h>
#include <message_api/ArrasTime.h>
#include <message_api/Address.h>

using namespace arras4::network;

namespace arras4
{
    namespace impl {

InStreamImpl::InStreamImpl(DataSource& source)
    :  mSource(source)
{
}

size_t InStreamImpl::read(void* aBuf, size_t aLen)
{
    return mSource.read(reinterpret_cast<unsigned char*>(aBuf),aLen);
}
 
size_t InStreamImpl::skip(size_t aLen)
{
    return mSource.skip(aLen);
}

size_t InStreamImpl::bytesRead() const
{
    return mSource.bytesRead();
}


OutStreamImpl::OutStreamImpl(DataSink& sink)
    :  mSink(sink)
{
}

size_t OutStreamImpl::write(const void* aBuf, size_t aLen)
{
    return mSink.write(reinterpret_cast<const unsigned char*>(aBuf),aLen);
}
   
size_t OutStreamImpl::fill(unsigned char aByte,size_t aCount)
{
    size_t filled = 0;
    for (size_t i = 0; i < aCount; i++)
        filled += write(aByte);
    return filled;
}
 
void OutStreamImpl::flush()
{
    mSink.flush();
}
  
size_t OutStreamImpl::bytesWritten() const
{
    return mSink.bytesWritten();
}

// Currently all types except strings are serialized using the internal memory 
// representation of the C++ runtime. This assumes it is the same across all 
// architectures we are using.

size_t InStreamImpl::read(bool& val)               { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(int8_t& val)             { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(int16_t& val)            { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(int32_t& val)            { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(int64_t& val)            { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(uint8_t& val)            { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(uint16_t& val)           { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(uint32_t& val)           { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(uint64_t& val)           { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(float& val)              { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(double& val)             { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(api::UUID& val)          { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(api::ArrasTime& val)     { return read(&val,sizeof(val)); }
size_t InStreamImpl::read(api::Address& val)       { return read(&val,sizeof(val)); }

size_t OutStreamImpl::write(bool val)                  { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(int8_t val)                { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(int16_t val)               { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(int32_t val)               { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(int64_t val)               { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(uint8_t val)               { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(uint16_t val)              { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(uint32_t val)              { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(uint64_t val)              { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(float val)                 { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(double val)                { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(const api::UUID& val)      { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(const api::ArrasTime& val) { return write(&val,sizeof(val)); }
size_t OutStreamImpl::write(const api::Address& val)   { return write(&val,sizeof(val)); }

namespace {

// helper functions to read and write strings using a count field of type LengthType 
// -- a numeric type not larger than size_t
template <typename LengthType> size_t readString(InStreamImpl& is,std::string& val)
{
    LengthType length = static_cast<LengthType>(val.length());
    size_t bytesRead = is.read(length);
    size_t stLen = static_cast<size_t>(length);
    if (stLen > 0) {
        std::string newString;
        newString.resize(stLen); // this can throw std::bad_alloc
        val.swap(newString);
        void* buf = &val.at(0);
        bytesRead += is.read(buf,stLen);
    } else {
        val.clear();
    }
    return bytesRead;
}

template <typename LengthType> size_t writeString(OutStreamImpl& os,const std::string& val)
{
    LengthType length = static_cast<LengthType>(val.length());
    size_t bytesWritten = os.write(length);
    return bytesWritten + os.write((const void*)val.c_str(),
                                   static_cast<size_t>(length));
}

} // namespace {}

size_t InStreamImpl::read(std::string& val)
{
    return readString<unsigned int>(*this,val);
}

size_t InStreamImpl::readLongString(std::string& val)
{
    return readString<size_t>(*this,val);
}

size_t OutStreamImpl::write(const std::string& val)
{
    return writeString<unsigned int>(*this,val);
}

size_t OutStreamImpl::writeLongString(const std::string& val)
{
    return writeString<size_t>(*this,val);
}

} 
} 

