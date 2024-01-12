// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS_DATASOURCE_H__
#define __ARRAS_DATASOURCE_H__

#include "network_types.h"
#include <cstddef>

namespace arras4 {
    namespace network {

// a source that can deliver blocks of data when requested
// Specific subclasses may allow you to set a timeout.
class DataSource
{
public:    
    virtual ~DataSource() {}

    // consume a block of data, copying it to aBuf. Returns
    // number of bytes read, or 0 on timeout.
    virtual size_t read(unsigned char* aBuf, size_t aLen)=0;
    // skip over and consume aLen bytes. Returns
    // number of bytes skipped, or 0 on timeout.
    virtual size_t skip(size_t aLen)=0;
    // return count bytes consumed
    virtual size_t bytesRead() const = 0;

};

// a source that delivers data within a framing protocol.
// Framing can aid buffering, and keeps the stream synchronized
// in the case of a read error (endFrame/nextFrame always discards
// unused bytes in the current frame).
// For a framed source, bytesRead() returns a count
// within the current frame, and reading or skipping
// beyond the current frame causes a FramingError exception.
class FramedSource :public DataSource
{
public:

    // request the next frame, return its size.
    // returns 0 if a timeout occurs before the next frame arrives
    virtual size_t nextFrame()=0;

    // indicate that no more data will be read 
    // from the current frame (may cause buffer release,
    // and/or check for excess unread data)
    virtual void endFrame()=0;
};

// an interface that allows the buffer to be detached from a buffered
// FramedSource. This avoids copying when the data needs to be kept for later use
class DetachableBufferSource : public FramedSource 
{
public:
    // returns the internal buffer from this buffered DataSource,
    // detaching it so that the source can no longer modify or
    // delete it.
    virtual BufferPtr takeBuffer()=0;

    // write buffer to file. Returns false if write fails.
    virtual bool writeToFile(const std::string& filepath)=0; 
};
    

}
}
#endif
