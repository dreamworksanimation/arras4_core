// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_BUFFERH__
#define __ARRAS4_BUFFERH__

#include "DataSource.h"
#include "DataSink.h"

#include <cstring>
#include <algorithm>
#include <iostream>

namespace arras4 {
    namespace network {

// Buffer stores a chunk of memory, with internal start and end points.:
//
//     <------------------capacity---------------------->
//   initial                                          final        
//     |................................................|
//     |...........|xxxxxxxxxxxxxxx|....................|
//     |         start            end                   |
//     |<-consumed-><--remaining---><-remainingCapacity->
//
// Data is typically written at 'end', and read at 'start'.
// 
// 'consumed' is the number of bytes that have been 
//            written and read
// 'remaining' is the number of bytes that have been
//            written but not yet read
// 'remainingCapacity' is the number unused bytes
//            available for writing
//
// Buffer clips reads & writes to the appropriate bounds.
class Buffer : public DataSource, public DataSink
{
public:
    // allocate a buffer on the heap.
    // throws std::bad_alloc if buffer allocation fails
    Buffer(size_t capacity) :
        mOwnsData(true),
        mData(new unsigned char[capacity]),
        mFinal(mData + capacity),
        mStart(mData),
        mEnd(mData)
    {}

    // provides the buffer interface on an existing data block.
    // does not delete the passed in data block. 
    // Pass filled=0 for an empty 'write' buffer, filled=length 
    // for a prefilled 'read' buffer.
    Buffer(unsigned char* data, size_t length, size_t filled) :
        mOwnsData(false),
        mData(data),
        mFinal(mData + length),
        mStart(mData),
        mEnd(mData + filled)
        {}

    ~Buffer() { if (mOwnsData) delete[] mData; }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    void copyIntoString(std::string& out) const { 
        out.assign((char*)(mStart),remaining());
    }

    void reset() {
        mStart = mData; mEnd = mData; 
    }

    void releaseData() {
        mOwnsData = false;
    }

    size_t write(const unsigned char* data, size_t length) {
        length = std::min(length,static_cast<size_t>(mFinal-mEnd));
        if (length > 0)
            std::memcpy(mEnd,data,length);
        mEnd += length;
        return length;
    }
     
    size_t read(unsigned char* target, size_t length) {
        length = std::min(length,static_cast<size_t>(mEnd-mStart));
        if (length > 0)
            std::memcpy(target,mStart,length);
        mStart += length;
        return length;
    }

    size_t skip(size_t length) { 
        length = std::min(length,static_cast<size_t>(mEnd-mStart));
        mStart += length;
        return length;
    }

    size_t seek(size_t pos) {
        pos = std::min(pos,static_cast<size_t>(mEnd-mData));
        mStart = mData+pos;
        return pos;
    }

    size_t assign(unsigned char*& target,size_t length) {
        target = mEnd;
        length = std::min(length,static_cast<size_t>(mFinal-mEnd));
        mEnd += length; 
        return length; 
    }

    void flush() {}

    unsigned char* initial() { return mData; }
    const unsigned char* initial() const { return mData; }
    unsigned char* start() { return mStart; }
    const unsigned char* start() const { return mStart; }
    unsigned char* end()  { return mEnd; }
    const unsigned char* end() const { return mEnd; }
    size_t capacity() const { return mFinal - mData; }
    size_t remaining() const { return mEnd - mStart; }
    size_t remainingCapacity() const { return mFinal - mEnd; }
    size_t consumed() const { return mStart - mData; }
    
    size_t bytesRead() const { return mStart-mData; }
    size_t bytesWritten() const { return mEnd-mData; }

private:
    bool mOwnsData;

    unsigned char* mData;
    unsigned char* mFinal;
    unsigned char* mStart;
    unsigned char* mEnd;
};

    }
}
#endif
    
