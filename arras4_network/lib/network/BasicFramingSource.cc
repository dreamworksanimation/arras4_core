// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "BasicFramingSource.h"
#include "FramingError.h"
#include "Frame.h"

namespace arras4 {
    namespace network {


BasicFramingSource::BasicFramingSource(DataSource& inputSource) :
    mInputSource(inputSource)
{
}

BasicFramingSource::~BasicFramingSource()
{
}

size_t BasicFramingSource::bytesRead() const
{
    return mBytesRead;
}

size_t BasicFramingSource::read(unsigned char* aBuf, size_t aLen)
{
    if (aLen > remaining())
        throw FramingError("Attempt to read beyond end of data frame");
    size_t read = mInputSource.read(aBuf, aLen);
    mBytesRead += read;
    return read;
}

size_t BasicFramingSource::skip(size_t aLen)
{ 
    if (aLen > remaining())
        throw FramingError("Attempt to skip beyond end of data frame");
    size_t skipped = mInputSource.skip(aLen);
    mBytesRead += skipped;
    return skipped;
}


size_t BasicFramingSource::nextFrame()
{
   Frame frameHdr;
   size_t r = mInputSource.read(reinterpret_cast<unsigned char*>(&frameHdr),sizeof(frameHdr));
   if (r) {
       mFrameSize = frameHdr.mLength;
       mBytesRead = 0;
       return mFrameSize;
   } else {
       // timeout
       return 0;
   }
}

void BasicFramingSource::endFrame()
{
    mFrameSize = 0;
    mBytesRead = 0;
}


}
}
  
