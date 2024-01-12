// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "BasicFramingSink.h"
#include "FramingError.h"
#include "Frame.h"

#include <limits>

namespace arras4 {
    namespace network {


BasicFramingSink::BasicFramingSink(DataSink& outputSink) :
    mOutputSink(outputSink)
{
}

BasicFramingSink::~BasicFramingSink()
{
}

size_t BasicFramingSink::bytesWritten() const
{
    return mBytesWritten;
}

size_t BasicFramingSink::write(const unsigned char* aBuf, size_t aLen)
{
    if (aLen > remaining())
        throw FramingError("Attempt to write beyond end of data frame");
    mOutputSink.write(aBuf, aLen);
    mBytesWritten += aLen;
    return aLen;
}

void BasicFramingSink::flush()
{
    mOutputSink.flush();
}

bool BasicFramingSink::openFrame(size_t frameSize)
{
    if (frameSize > std::numeric_limits<unsigned int>::max())
        throw FramingError("Data is too long for the framing protocol. Limit is ~2Gb");
    
    Frame frameHdr;
    frameHdr.mType = Frame::FRAME_BINARY;
    frameHdr.mLength = (unsigned)frameSize;
    frameHdr.mReserved1 = frameHdr.mReserved2 = 0;
    size_t w = mOutputSink.write(reinterpret_cast<unsigned char*>(&frameHdr), sizeof(frameHdr));
    if (w) {
        mFrameSize = frameSize;
        mBytesWritten = 0;
    }
    return w;
}

// enables the internal buffer to be freed
bool BasicFramingSink::closeFrame()
{
    if (remaining() != 0) 
        throw FramingError("Not enough data written to fill frame");
    mFrameSize = 0;
    mBytesWritten = 0;
    return true;
}


}
}
  
