// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_BUFFERED_SOURCEH__
#define __ARRAS4_BUFFERED_SOURCEH__

#include "network_types.h"
#include "DataSource.h"

namespace arras4 {
    namespace network {


// BufferedSource buffers the data requests from a framed source 
// into larger chunks, so that small requests (e.g those typically
// coming from serialization functions) are handled efficiently.
//
// It is designed to buffer an entire frame from its source.
// The frame buffer can be removed rather than copied, using
// the takeBuffer() function. This allows efficient transfer of
// opaque data from the source to a sink without additional
// copying. See also BufferedSink::appendBuffer().
//
// BufferedSource supports timeouts in the input source, 
// i.e. if inputSource.nextFrame() returns 0, then 
// BS::nextFrame will return 0 and can be called again.
//
class BufferedSource : public DetachableBufferSource
{
public:
    BufferedSource(FramedSource& inputSource);
    ~BufferedSource();

    BufferedSource(const BufferedSource&) = delete;
    BufferedSource& operator=(const BufferedSource&) = delete;
    BufferedSource(BufferedSource&&) = delete;
    BufferedSource& operator=(BufferedSource&&) = delete;
    
    // required DataSource interface. reads data from
    // the buffer that has previously been filled
    size_t read(unsigned char* aBuf, size_t aLen);
    size_t skip(size_t aLen);
    size_t bytesRead() const;

    // BufferedSource supports framing : each frame is one message
    size_t nextFrame();
    void endFrame();
    
    // returns the internal buffer,
    // releasing it from use by this object
    BufferPtr takeBuffer();

    // shrink buffer capacity to less than or equal to
    // 'maxCapacity' by removing the buffer if it is
    // bigger. Will not remove the buffer if it is in use, 
    // so generally called after endFrame()
    void shrinkTo(size_t maxCapacity);

    // write buffer to file. Returns false if write fails.
    bool writeToFile(const std::string& filepath);

private:
    unsigned char* prepForFill(size_t size);
    void newBuffer(size_t size);

    FramedSource& mInputSource;
    bool mIsInFrame = false;

    BufferPtr mBuffer;
   
};

}
}
#endif
  
