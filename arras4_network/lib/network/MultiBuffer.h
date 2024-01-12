// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_MULTIBUFFERH__
#define __ARRAS4_MULTIBUFFERH__

#include "network_types.h"

#include "DataSource.h"
#include "DataSink.h"

#include <vector>

namespace arras4 {
    namespace network {
// size of initial buffer allocation (1Mb)
constexpr size_t INITIAL_BUFFER_SIZE = 1024*1024;
// size at which new buffer allocation becomes linear (1Gb)
constexpr size_t LINEAR_BUFFER_SIZE = 1024*1024*1024;

// MultiBuffer can accept an indefinite sequences of writes, growing in size as
// needed. It does this by storing the data in a sequence of individual buffers,
// which avoids the need to copy data across as the buffer size grows.
// 
// The first buffer has size 'initialSize'. Subsequent buffers then double in size
// until 'linearSize' is reached. After this, each new buffer is always
// 'linearSize'.
//
// To access the written data, you can either use read(), iterate through the buffers, via
// bufferCount()/buffer(i) or copy the data into a single buffer via 'collect'.
class MultiBuffer : public DataSource, public DataSink
{
public:
    MultiBuffer(size_t initialSize=INITIAL_BUFFER_SIZE,
                size_t linearSize=LINEAR_BUFFER_SIZE);

    ~MultiBuffer();

    MultiBuffer(const MultiBuffer&) = delete;
    MultiBuffer& operator=(const MultiBuffer&) = delete;
    MultiBuffer(MultiBuffer&&) = delete;
    MultiBuffer& operator=(MultiBuffer&&) = delete;

    size_t write(const unsigned char* aBuf, size_t aLen);
    size_t bytesWritten() const { return mBytesWritten; }

    size_t read(unsigned char* aBuf, size_t aLen);
    size_t skip(size_t length);
    void flush() {}
    size_t bytesRead() const { return mBytesRead; }

    // collect the data into 'out', stopping when out
    // runs out of capacity
    void collect(Buffer& out);

    // reset the write pointer, without deallocating buffers
    void reset();

    // add a buffer. This fails and returns false if
    // the current last buffer is not in use. Generally
    // you would add buffers just after construction
    bool addBuffer(BufferUniquePtr&& buf);

    // shrink buffer capacity to less than or equal to
    // 'maxCapacity' by removing buffers. Will not
    // remove buffers that are in use, so generally
    // called after reset(). Appended buffer size
    // is not taken into account.
    void shrinkTo(size_t maxCapacity);
    
    size_t bufferCount() { return mUsedBuffers; }
    const BufferUniquePtr& buffer(size_t index) { return mBuffers[index]; }
    BufferUniquePtr&& takeBuffer(size_t index) { return std::move(mBuffers[index]); }

private:
    void useNextBuffer();
    size_t nextSize(size_t size);

    // initial size of buffer to allocate
    size_t mInitialSize;

    // size at which allocation switches to linear
    size_t mLinearSize;

    // total number of bytes in use, across all buffers
    size_t mBytesWritten;

    // current read pointers
    size_t mReadBuffer; // index of current buffer being read
    size_t mBytesRead; // total bytes read across all buffers

    // number of buffers that are in use
    size_t mUsedBuffers;
   
    std::vector<BufferUniquePtr> mBuffers;
};

}
}
#endif
