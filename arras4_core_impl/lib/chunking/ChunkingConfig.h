// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_CHUNKING_CONFIGH__
#define __ARRAS4_CHUNKING_CONFIGH__

#include <stddef.h>

namespace arras4 {
    namespace impl {

class ChunkingConfig
{
public:
    bool enabled = true;
    // default settings are to avoid messages > 2GB in size, since
    // these are not supported by Arras 3
    size_t minChunkingSize =  2047 * 1024  * 1024ull; // 2GB - 1MB
    size_t chunkSize =  1024 * 1024  * 1024ull; // 1GB
};

}
}
#endif
