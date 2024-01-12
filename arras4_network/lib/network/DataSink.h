// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_DATASINK_H__
#define __ARRAS_DATASINK_H__

#include "network_types.h"
#include <cstddef>

namespace arras4
{
    namespace network {

// a sink that can receive blocks of data
class DataSink
{
public:
    virtual ~DataSink() {}
    // write out a block of data read from aBuf. Returns
    // number of bytes written, or 0 on timeout.
    virtual size_t write(const unsigned char* aBuf, size_t aLen) = 0;
    virtual void flush() = 0;
    virtual size_t bytesWritten() const=0;

};

// a sink that delivers data within a framing protocol.
// Framing can aid buffering, and keeps the stream synchronized
// in the case of a read error.
// For a framed sink, bytesWritten() returns a count
// within the current frame, and writing beyond the current frame
// causes a FramingError exception.
// Specific subclasses may allow you to set timeouts.
class FramedSink : public DataSink
{
public:
    // begin a new frame, specifying its size. Subsequent writes
    // will go into the new frame. Returns false if a timeout
    // occurs before the frame can be opened, and may then be called
    // again.
    virtual bool openFrame(size_t frameSize)=0;

    // indicate that the current frame has ended, and there
    // will be no more writes until openFrame is called again.
    // (may cause buffer release and/or check for insufficient
    // data written). Returns false if a timeout
    // occurs before the frame can be closed, and may then be called
    // again.
    virtual bool closeFrame()=0;
};

// Similar to a framed sink, but it is not necessary to specify
// the size of the frame when it is opened : it will be deduced by
// the number of bytes written between openFrame/closeFrame.
// Typically implemented on top of a regular FramedSink using 
// buffering. You can also append additional buffers to the sink.
class AttachableBufferSink : public DataSink
{
public:
    // begin a new frame. Subsequent writes
    // will go into the new frame. Returns false if a timeout
    // occurs before the frame can be opened, and may then be called
    // again.
    virtual bool openFrame()=0;

    // indicate that the current frame has ended, and there
    // will be no more writes until openFrame is called again.
    // (may cause buffer release and/or check for insufficient
    // data written). Returns false if a timeout
    // occurs before the frame can be closed, and may then be called
    // again.
    virtual bool closeFrame()=0;

    virtual void appendBuffer(const BufferConstPtr& buf)=0;

    // writes all buffers to file. Returns false if write fails.
    virtual bool writeToFile(const std::string& filepath)=0;  
};
}
}
#endif
