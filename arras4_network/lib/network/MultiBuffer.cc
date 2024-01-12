// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "MultiBuffer.h"
#include "Buffer.h"
#include "OutOfMemoryError.h"

#include <algorithm>

namespace arras4 {
    namespace network {


MultiBuffer::MultiBuffer(size_t initialSize,
                         size_t linearSize)
    :  mInitialSize(initialSize),
       mLinearSize(linearSize),
       mBytesWritten(0),
       mReadBuffer(0),
       mBytesRead(0),
       mUsedBuffers(0)
{
    
}

MultiBuffer::~MultiBuffer()
{
}

size_t MultiBuffer::nextSize(size_t size)
{
    if (size == 0) {
        return mInitialSize;
    } else if (size < mLinearSize) {
        return size*2;
    } else {
        return size;
    }
}

void MultiBuffer::reset()
{
    mBytesWritten = 0;
    mBytesRead = 0;
    mUsedBuffers = 0;
    mReadBuffer = 0;
}
 
size_t MultiBuffer::read(unsigned char* aBuf, size_t aLen)
{
    size_t remaining = aLen;
    size_t offset = 0;
    while ((mReadBuffer <= mUsedBuffers) &&
           remaining) 
    {
        const BufferUniquePtr& buf = mBuffers[mReadBuffer];
        if (buf->remaining() == 0) {
            mReadBuffer++;
            continue;
        }
        size_t toRead = std::min(remaining,buf->remaining());       
        buf->read(aBuf + offset,toRead);
        offset += toRead;
        remaining -= toRead;        
    }
    mBytesRead += offset;
    return offset;
}

size_t MultiBuffer::skip(size_t aLen)
{
    size_t remaining = aLen;
    size_t skipped = 0;
    while ((mReadBuffer <= mUsedBuffers) &&
           remaining) 
    {
        const BufferUniquePtr& buf = mBuffers[mReadBuffer];
        if (buf->remaining() == 0) {
            mReadBuffer++;
            continue;
        }
        size_t toSkip = std::min(remaining,buf->remaining());
        skipped += toSkip;
        remaining -= toSkip;
    }
    mBytesRead += skipped;
    return skipped;
}

void MultiBuffer::collect(Buffer& out)
{
    for (size_t i = 0; i < mUsedBuffers; i++) {
        if (out.remainingCapacity() == 0)
            break;
        const BufferUniquePtr& buf = mBuffers[i];
        // will automatically be clipped to the end of out
        out.write(buf->start(),buf->remaining());
    }                    
}
    
size_t MultiBuffer::write(const unsigned char* aBuf, size_t aLen)
{
    size_t remaining = aLen;
    const unsigned char* data = static_cast<const unsigned char*>(aBuf);
    while (remaining > 0) {

        // if the current end buffer is full, move to
        // the next one...
        if (mUsedBuffers == 0 ||
            mBuffers[mUsedBuffers-1]->remainingCapacity() <= 0) {
            useNextBuffer();
        }

        // write as much as possible into the current end buffer
        Buffer* buf = mBuffers[mUsedBuffers-1].get();       
        size_t toWrite = std::min(remaining, buf->remainingCapacity());
        buf->write(data,toWrite);
        remaining -= toWrite;
        data += toWrite;
    }
    mBytesWritten += aLen;
    return aLen;
}

bool MultiBuffer::addBuffer(BufferUniquePtr&& buf)
{
    if (mUsedBuffers == mBuffers.size()) {
        mBuffers.push_back(std::move(buf));
        mUsedBuffers++;
        return true;
    }
    return false;
}    

void MultiBuffer::useNextBuffer()
{
   
    size_t nextBuffer = mUsedBuffers;
    if (nextBuffer >= mBuffers.size()) {
        // run out of buffers, need to allocate a new one
        size_t newSize = mInitialSize;
        if (nextBuffer > 0)
            newSize = nextSize(mBuffers[nextBuffer-1]->capacity());
        try {
            Buffer* newBuffer = new Buffer(newSize);
            mBuffers.push_back(BufferUniquePtr(newBuffer));
        } catch (std::bad_alloc&) {
            throw OutOfMemoryError("Write buffer allocation failed : out of memory?");
        }
    } else {
        mBuffers[nextBuffer]->reset();
    }
    mUsedBuffers++;
}
      
void MultiBuffer::shrinkTo(size_t maxCapacity)
{
    // remove as many buffers from the end of mBuffers
    // as are necessary to reduce the capacity to
    // less than or equal to 'maxCapacity'
   
    // find the first buffer that causes
    // maxCapacity to be exceeded
    size_t capacity = 0;
    size_t target;
    for (target = 0; 
         target < mBuffers.size();
         target++) {
        capacity += mBuffers[target]->capacity();
        if (capacity > maxCapacity)
            break;
    }

    // we can only remove unused buffers
    target = std::max(target,mUsedBuffers);
    if (target < mBuffers.size()) {
        mBuffers.resize(target);
    }
}

}
}
