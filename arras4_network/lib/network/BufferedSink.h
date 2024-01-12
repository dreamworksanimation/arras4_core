// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_BUFFERED_SINKH__
#define __ARRAS4_BUFFERED_SINKH__

#include "network_types.h"
#include "DataSink.h"
#include "MultiBuffer.h"

#include <vector>


namespace arras4 {
    namespace network {

// Provides memory buffering for data output.
//
// BufferedSink's job is twofold :
//
//   1) to accumulate small writes into larger blocks 
//      (making transfer through its output sink more efficient)
//
//   2) to allow framing (one frame per message) without having
//      to specify the frame size up front. This makes message
//      content serialization easier for clients to implement.
//
// 2) means that BufferedSink implements the AutoframedSink
// interface
//
// BufferedSink also allows a complete buffer to be appended without
// copying. This is used to efficiently transfer opaque content from
// a source to a sink (see also BufferedSource::takeBuffer).
class BufferedSink : public AttachableBufferSink
{
public:
    BufferedSink(FramedSink& output);
    ~BufferedSink();

    BufferedSink(const BufferedSink&) = delete;
    BufferedSink& operator=(const BufferedSink&) = delete;
    BufferedSink(BufferedSink&&) = delete;
    BufferedSink& operator=(BufferedSink&&) = delete;

    // required DataSink interface. 
    // May throw OutOfMemoryError.
    size_t write(const unsigned char* aBuf, size_t aLen);
    void flush();
    size_t bytesWritten() const { return mMultiBuffer.bytesWritten() + mAppendedLength; }

    // provides an autoframed sink, with each frame being a
    // complete message.
    bool openFrame();
    bool closeFrame();

    // has the additional ability to append an existing
    // buffer to the frame : used for efficient transfer 
    // of opaque messages
    void appendBuffer(const BufferConstPtr& buf);
     
    // shrink buffer capacity to less than or equal to
    // 'maxCapacity' by removing buffers. Will not
    // remove buffers that are in use, so generally
    // called after endFrame()
    void shrinkTo(size_t maxCapacity);
    
    // writes all buffers to file. Returns false if write fails.
    bool writeToFile(const std::string& filepath);
  
private:
    void reset();

    FramedSink& mOutputSink;    
    MultiBuffer mMultiBuffer;

    // additional appended buffers
    size_t mAppendedLength;
    std::vector<BufferConstPtr> mAppendedBuffers;
};

}
}
#endif
