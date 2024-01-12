// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_BASIC_FRAMING_SOURCEH__
#define __ARRAS4_BASIC_FRAMING_SOURCEH__

#include "DataSource.h"

namespace arras4 {
    namespace network {

// Adds framing to an unframed source, using the Arras BasicFraming protocol
// Propagates timeouts in the input source (e.g. if input source read returns
// 0, BFS::nextFrame will return 0, and can be called again)
class BasicFramingSource : public FramedSource
{
public:
    BasicFramingSource(DataSource& inputSource);
    ~BasicFramingSource();
    
    // required FramedSource interface. reads data the
    // current frame, throws a FramingError if
    // the frame is exceeded
    size_t read(unsigned char* aBuf, size_t aLen);
    size_t skip(size_t aLen);
    size_t bytesRead() const;
    size_t nextFrame();
    void endFrame();

private:

    size_t remaining() { return mFrameSize - mBytesRead; }

    DataSource& mInputSource;
    size_t mBytesRead = 0;
    size_t mFrameSize = 0;
};

}
}
#endif
  
