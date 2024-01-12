// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "BufferedSink.h"
#include "Buffer.h"
#include "OutOfMemoryError.h"

#include <algorithm>
#include <fstream>

// BufferedSink needs the ability to store an arbitrarily large amount of data
// in memory, because it does not pass data on to its output sink until it
// knows how large the current frame is. A frame contains an entire message
// and therefore is arbitrarily large.
//
// One contiguous buffer would require complete reallocation and copying every
// time it needed to grow. There is no reason that the data has to be stored
// in one block, so instead we use a MultiBuffer
namespace arras4 {
    namespace network {


BufferedSink::BufferedSink(FramedSink& outputSink)
    :  mOutputSink(outputSink),
       mAppendedLength(0)
{
}

BufferedSink::~BufferedSink()
{
}

void BufferedSink::reset()
{
    mMultiBuffer.reset();
    mAppendedBuffers.clear();
    mAppendedLength = 0;
}
 
void BufferedSink::appendBuffer(const BufferConstPtr& buf)
{
    mAppendedBuffers.push_back(buf);
    mAppendedLength += buf->remaining();
}

bool BufferedSink::openFrame()
{
    reset();
    return true;
}

bool BufferedSink::closeFrame()
{
    // now the frame is ended, we can send it to our output sink
    bool ok = mOutputSink.openFrame(bytesWritten());
    if (!ok) return false; // timeout

    for (size_t i = 0; i < mMultiBuffer.bufferCount(); i++) {
        const BufferUniquePtr& buf = mMultiBuffer.buffer(i);
        mOutputSink.write(buf->start(),buf->remaining());
    }                    
        
    for (size_t i = 0; i < mAppendedBuffers.size(); i++) {
        const BufferConstPtr& buf = mAppendedBuffers[i];
        mOutputSink.write(buf->start(),buf->remaining());
    }
                    
    mOutputSink.closeFrame();
    reset();
    return true;
}
    
size_t BufferedSink::write(const unsigned char* aBuf, size_t aLen)
{
    size_t written = mMultiBuffer.write(aBuf,aLen);
    return written;
}

void BufferedSink::shrinkTo(size_t maxCapacity)
{
    mMultiBuffer.shrinkTo(maxCapacity);
}

// flush has no effect, since we can't send anything until 
// closeFrame()   
void BufferedSink::flush()
{
}

bool BufferedSink::writeToFile(const std::string& filepath)
{
    std::ofstream ofs(filepath.c_str());
    if (!ofs) return false; 
    for (size_t i = 0; i < mMultiBuffer.bufferCount(); i++) {
        const BufferUniquePtr& buf = mMultiBuffer.buffer(i);
        ofs.write(reinterpret_cast<const char*>(buf->start()),buf->remaining());
        if (!ofs) return false;
    }                    
        
    for (size_t i = 0; i < mAppendedBuffers.size(); i++) {
        const BufferConstPtr& buf = mAppendedBuffers[i];
        ofs.write(reinterpret_cast<const char*>(buf->start()),buf->remaining());
        if (!ofs) return false;
    }
    ofs.close();
    return bool(ofs);
}

}
}
