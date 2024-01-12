// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_STREAMIMPL_H__
#define __ARRAS_STREAMIMPL_H__

#include <network/network_types.h>

#include <message_api/DataInStream.h>
#include <message_api/DataOutStream.h>

namespace arras4
{
    namespace impl {

class InStreamImpl : public api::DataInStream
{
public:
    InStreamImpl(network::DataSource& source);

    size_t read(void* aBuf, size_t aLen);
    size_t skip(size_t aLen); 
    size_t bytesRead() const;

    size_t read(bool& val);
    size_t read(int8_t& val);
    size_t read(int16_t& val);
    size_t read(int32_t& val);
    size_t read(int64_t& val);
    size_t read(uint8_t& val);
    size_t read(uint16_t& val);
    size_t read(uint32_t& val);
    size_t read(uint64_t& val);
    size_t read(float& val);
    size_t read(double& val);
    size_t read(std::string& val);           
    size_t readLongString(std::string& val);
    size_t read(api::UUID& uuid);
    size_t read(api::ArrasTime& time);
    size_t read(api::Address& address);

protected:
    network::DataSource& mSource;

};

class OutStreamImpl : public api::DataOutStream
{
public:
    OutStreamImpl(network::DataSink& sink);

    size_t write(const void* aBuf, size_t aLen);
    size_t fill(unsigned char aByte,size_t aCount);
    void flush();
    size_t bytesWritten() const;

    size_t write(bool val);
    size_t write(int8_t val);
    size_t write(int16_t val);
    size_t write(int32_t val);
    size_t write(int64_t val);
    size_t write(uint8_t val);
    size_t write(uint16_t val);
    size_t write(uint32_t val);
    size_t write(uint64_t val);
    size_t write(float aFloat);
    size_t write(double aDouble);
    size_t write(const std::string& aString);           
    size_t writeLongString(const std::string& aString);   
    size_t write(const api::UUID& uuid);
    size_t write(const api::ArrasTime& time);
    size_t write(const api::Address& address);


protected:
    network::DataSink& mSink;

};

}
}
#endif
