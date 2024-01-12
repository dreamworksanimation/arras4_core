// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_HTTPSERVER_TYPESH__
#define __ARRAS4_HTTPSERVER_TYPESH__

#include <string>
#include <map>

namespace arras4 {
    namespace network {

class HttpServerResponse;
class HttpServerRequest;

// original code uses an arras::object::Object, and so
// doesn't handle headers as case-insensitive
// or query params as repeatable. Keeping this
// behavior for now.
typedef std::map<std::string, 
                 std::string> StringMap;

typedef int ServerResponseCode;

}
}
#endif
