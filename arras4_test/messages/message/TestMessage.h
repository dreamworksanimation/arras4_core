// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_TEST_MESSAGEH__
#define __ARRAS4_TEST_MESSAGEH__

#include <message_api/ContentMacros.h>
#include <string>

namespace arras4 {
    namespace test {

// defer count (0-7) is decreased by one every serialize :
// -- error is only forced when it reaches zero
constexpr unsigned DEFERMASK = 7;

// values for forced error : type of error (exclusive options)
constexpr unsigned ERRTYPEMASK = 3 * 8;
constexpr unsigned THROW =    1 * 8;
constexpr unsigned SEGFAULT = 2 * 8;
constexpr unsigned CORRUPT =  3 * 8;

// place to force error (additive options)
constexpr unsigned IN_SERIALIZE =  32;
constexpr unsigned IN_DESERIALIZE = 64;


class TestMessage : public arras4::api::ObjectContent
{
public:

    ARRAS_CONTENT_CLASS(TestMessage,"7680a6e1-a00b-4652-8065-c7ffb3a35265",0);
    
    TestMessage() {}
    TestMessage(unsigned index,
                const std::string& text = std::string(),
                size_t dataSize = 0) :
        mIndex(index),
        mText(text) {
        if (dataSize) 
            setRandomData(dataSize);
    }

    TestMessage(const TestMessage&);

    ~TestMessage() { delete mData; }

    void serialize(arras4::api::DataOutStream& to) const;
    void deserialize(arras4::api::DataInStream& from, unsigned version);

    // for testing message chunking : generally not required in message impls
    size_t serializedLength() const;
    
    unsigned forcedErrors() const { return mForcedErrors; }
    unsigned& forcedErrors() { return mForcedErrors; }
    const std::string& text() const { return mText; }
    std::string& text() { return mText; }
    unsigned index() const { return mIndex; }
    void setRandomData(size_t size);
    void makeChecksumWrong();
    size_t dataSize() const { return mDataSize; }
    std::string describe() const;

private:
    unsigned mIndex;
    std::string mText;
    size_t mDataSize = 0;
    unsigned char* mData = nullptr;
    unsigned char mMD5[16] = {0};
    unsigned mForcedErrors = 0;
};

}
}
#endif
