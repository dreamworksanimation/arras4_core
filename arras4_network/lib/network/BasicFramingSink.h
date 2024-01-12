// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_BASIC_FRAMING_SINKH__
#define __ARRAS4_BASIC_FRAMING_SINKH__

#include "DataSink.h"

namespace arras4 {
    namespace network {

// Adds framing to an unframed sink,
// using the Arras BasicFraming protocol
class BasicFramingSink : public FramedSink
{
public:
    BasicFramingSink(DataSink& outputSink);
    ~BasicFramingSink();
    
    // required FramedSink interface. writes
    // data into the current frame, throws a 
    // FramingError if the frame is exceeded
    size_t write(const unsigned char* aBuf, size_t aLen);
    void flush();
    size_t bytesWritten() const;
    bool openFrame(size_t frameSize);
    bool closeFrame();

private:

    size_t remaining() { return mFrameSize - mBytesWritten; }

    DataSink& mOutputSink;
    size_t mBytesWritten = 0;
    size_t mFrameSize = 0;
};

}
}
#endif
  
