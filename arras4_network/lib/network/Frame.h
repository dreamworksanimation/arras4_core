// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_FRAMEH__
#define __ARRAS4_FRAMEH__

// definition of the header for arras BasicFraming
namespace arras4 {
    namespace network {

class Frame
{ 
public:
    enum FrameType
    {
        FRAME_BINARY,
        FRAME_UTF8,
        FRAME_UNKNOWN,
    };
    // whether this frame is binary or text (UTF-8)
    FrameType mType = FRAME_UNKNOWN;

    // length of this frame in bytes
    unsigned int mLength = 0;

    // reserved
    unsigned int mReserved1 = 0;
    unsigned int mReserved2 = 0;
};

}
}
#endif
