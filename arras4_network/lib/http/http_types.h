// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_HTTP_TYPESH__
#define __ARRAS4_HTTP_TYPESH__

#include <string>
#include <strings.h> // strcasecmp
#include <map>

namespace arras4 {
    namespace network {

class HttpResponse;
class HttpRequest;
class Buffer;

struct CaseInsensitiveLess {
    bool operator() (const std::string & s1, const std::string & s2) const {
        return strcasecmp(s1.c_str(),s2.c_str()) < 0;
    }
};

typedef std::map<std::string, 
                 std::string, 
                 CaseInsensitiveLess> Headers;
typedef std::multimap<std::string, 
                      std::string> Parameters;

constexpr const char* HTTP_CONTENT_TYPE = "Content-Type";

enum ResponseCode {
    HTTP_CONTINUE = 100,
    HTTP_SWITCHING_PROTOCOLS = 101,
    HTTP_PROCESSING = 102,
    HTTP_OK = 200,
    HTTP_CREATED = 201,
    HTTP_ACCEPTED = 202,
    HTTP_NON_AUTHORITATIVE_INFORMATION = 203,
    HTTP_NO_CONTENT = 204,
    HTTP_RESET_CONTENT = 205,
    HTTP_PARTIAL_CONTENT = 206,
    HTTP_MULTI_STATUS = 207,
    HTTP_MULTIPLE_CHOICES = 300,
    HTTP_MOVED_PERMANENTLY = 301,
    HTTP_FOUND = 302,
    HTTP_SEE_OTHER = 303,
    HTTP_NOT_MODIFIED = 304,
    HTTP_USE_PROXY = 305,
    HTTP_SWITCH_PROXY = 306,
    HTTP_TEMPORARY_REDIRECT = 307,
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_PAYMENT_REQUIRED = 402,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_METHOD_NOT_ACCEPTABLE = 406,
    HTTP_RESOURCE_CONFLICT = 409,
    HTTP_GONE = 410,
    HTTP_PRECONDITION_FAILED = 412,
    HTTP_TOO_MANY_REQUESTS = 429,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_NOT_IMPLEMENTED = 501,
    HTTP_SERVICE_UNAVAILABLE = 503,
    HTTP_INVALID_STATE = 999
};

enum HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS,
    PUT_MULTIPART,
};

enum HttpContentType {
    TEXT_PLAIN,
    TEXT_XML,
    TEXT_HTML,
    APPLICATION_JSON,
    APPLICATION_FORM_URL_ENCODED,
    APPLICATION_OCTET_STREAM,
    IMAGE_PNG
};

}
}
#endif
