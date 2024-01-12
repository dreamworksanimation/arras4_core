// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "BufferedSource.h"
#include "Buffer.h"
#include "FramingError.h"
#include "OutOfMemoryError.h"

#include <fstream>

namespace arras4 {
    namespace network {


BufferedSource::BufferedSource(FramedSource& inputSource) :
    mInputSource(inputSource)
{
}

BufferedSource::~BufferedSource()
{
}

size_t BufferedSource::bytesRead() const
{
    if (mBuffer)
        return mBuffer->consumed();
    return 0;
}

// take the current buffer away from the source,
// forcing it to create a new buffer on the next reset
BufferPtr BufferedSource::takeBuffer()
{
    BufferPtr ret;
    ret.swap(mBuffer);
    return ret;
}

size_t BufferedSource::read(unsigned char* aBuf, size_t aLen)
{
    if (!mBuffer)
        throw FramingError("Attempt to read without message data");
    if (aLen > mBuffer->remaining())
        throw FramingError("Attempt to read beyond end of message data");
    mBuffer->read(static_cast<unsigned char*>(aBuf), aLen);
    return aLen;
}

size_t BufferedSource::skip(size_t aLen)
{ 
    if (!mBuffer)
        throw FramingError("Attempt to skip without message data");
    if (aLen > mBuffer->remaining())
        throw FramingError("Attempt to skip beyond end of message data");
    mBuffer->skip(aLen);
    return aLen;
}


// used to reset the buffer and prep it for
// filling with 'size' bytes.
unsigned char* BufferedSource::prepForFill(size_t size)
{
    if (!mBuffer || size > mBuffer->capacity()) {
        try {
            mBuffer = BufferPtr(new Buffer(size));
        } catch (std::bad_alloc&) {
            throw OutOfMemoryError("Read buffer allocation failed : out of memory?");
        }
    }
    mBuffer->reset();
    unsigned char* fillAddr;
    mBuffer->assign(fillAddr,size);
    return fillAddr;
}

size_t BufferedSource::nextFrame()
{
    size_t frameSize = mInputSource.nextFrame();
    if (frameSize) { // frameSize==0 means a timeout occurred
        unsigned char* fillPtr = prepForFill(frameSize);
        mInputSource.read(fillPtr,frameSize);
        mInputSource.endFrame();
        mIsInFrame = true;
    } 
    return frameSize;
}

// enables the internal buffer to be freed
void BufferedSource::endFrame()
{
    mIsInFrame = false;
}

void BufferedSource::shrinkTo(size_t maxCapacity)
{
    if (!mIsInFrame && 
        mBuffer && 
        (mBuffer->capacity() > maxCapacity)) {
        mBuffer.reset();
    }
}

bool BufferedSource::writeToFile(const std::string& filepath)
{
    if (!mBuffer) return false;
    std::ofstream ofs(filepath.c_str());
    if (!ofs) return false;
    // writes all bytes from initial to end
    ofs.write(reinterpret_cast<char*>(mBuffer->initial()),
              mBuffer->consumed() + mBuffer->remaining());
    ofs.close();
    return bool(ofs);
}

}
}
  
