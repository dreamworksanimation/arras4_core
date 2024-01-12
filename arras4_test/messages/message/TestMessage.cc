// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "TestMessage.h"
#include <message_api/MessageFormatError.h>

#include <openssl/md5.h>
#include <unistd.h> 
#include <cstring>

namespace {

bool checkMD5(unsigned char* data,
              size_t len,
              unsigned char* md5)
{
    unsigned char buf[16];
    MD5(data,len,buf);
    for (unsigned i = 0; i < 16; i++) 
        if (md5[i] != buf[i]) return false;
    return true;
}

}

namespace arras4 {
    namespace test {

ARRAS_CONTENT_IMPL(TestMessage);
 
TestMessage::TestMessage(const TestMessage& copyMe) :
    mIndex(copyMe.mIndex),
    mText(copyMe.mText),
    mDataSize(copyMe.mDataSize),
    mForcedErrors(copyMe.mForcedErrors)
{
    if (mDataSize > 0) {
        mData = new unsigned char[mDataSize];
        std::memcpy(mData,copyMe.mData,mDataSize);
        std::memcpy(mMD5,copyMe.mMD5,16);
    }
}

size_t TestMessage::serializedLength() const
{
    return mDataSize + mText.length() + 36;
}

void TestMessage::serialize(arras4::api::DataOutStream& to) const
{

    to << mIndex << mText << mDataSize;

    unsigned fe = mForcedErrors;
    unsigned defer = fe & DEFERMASK;
    if (defer) {
        defer--;
        fe = (fe & ~DEFERMASK) | defer;
    } else if (fe & IN_SERIALIZE) {
        unsigned et = fe & ERRTYPEMASK;
        if (et == THROW) {
            throw std::runtime_error("Thrown for testing in 'serialize'");
        } else if (et == SEGFAULT) {
            int* ptr= nullptr;
            // cppcheck-suppress nullPointer
            *ptr = *ptr + 1;
        } else if (et == CORRUPT) {
            int dummy = 57;
            to << dummy;
            fe = 0;
        }
    }
    to << fe;

    to.write(mMD5,16);
    if (mDataSize > 0)
        to.write(mData,mDataSize);
}

void TestMessage::deserialize(arras4::api::DataInStream& from, 
                              unsigned)
{
    from >> mIndex >> mText >> mDataSize >> mForcedErrors;
   
    if ( (mForcedErrors & (IN_DESERIALIZE | DEFERMASK))
         == IN_DESERIALIZE) {
        unsigned et = mForcedErrors & ERRTYPEMASK;
        if (et == THROW) {
            throw std::runtime_error("Thrown for testing in 'deserialize'");
        } else if (et == SEGFAULT) {
            int* ptr= nullptr;
            // cppcheck-suppress nullPointer
            *ptr = *ptr + 1;
        } else if (et == CORRUPT) {
            int dummy;
            from >> dummy;
        }
    }

    from.read(mMD5,16);
    if (mDataSize > 0) {
        delete mData;
        mData = new unsigned char[mDataSize];
        from.read(mData,mDataSize);
        if (!checkMD5(mData,mDataSize,mMD5)) {
            throw arras4::api::MessageFormatError("Checksum failure in TestMessage");
        }
    }
}

void TestMessage::setRandomData(size_t size)
{
    mDataSize = 0;
    delete mData;
    mData = new unsigned char[size];
    mDataSize = size;

    // fast xor-shift rng
    unsigned x = rand();
    unsigned *p = (unsigned *)mData;
    for (size_t i = 0; i < size/4; i++) {
        *(p++) = x;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
    }
    MD5(mData,mDataSize,mMD5);
}
    
void TestMessage::makeChecksumWrong() 
{
    // used to force a deserialize error for testing...
    mMD5[0]++;
}

std::string TestMessage::describe() const
{
    std::string ret = "TM #" + std::to_string(mIndex) + " ";
    if (mDataSize) {
        ret += "["+std::to_string(mDataSize) + " bytes data] ";
    };
    ret += "\"" + mText + "\"";
    return ret;
}

}
}

