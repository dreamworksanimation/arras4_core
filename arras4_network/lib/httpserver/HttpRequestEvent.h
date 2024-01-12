// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef __ARRAS4_HTTP_REQUEST_EVENT_H__
#define __ARRAS4_HTTP_REQUEST_EVENT_H__

#include <string>
#include <list>
#include <functional>

namespace arras4 {
    namespace network {

        class HttpServerRequest;
        class HttpServerResponse;

class HttpRequestEvent {
public:
    typedef void handler_func(const HttpServerRequest&, HttpServerResponse&);
    typedef std::function<handler_func> Handler;

    HttpRequestEvent& operator+=(const Handler& handler) {
        mHandlers.push_back(handler);
        return *this;
    }

    HttpRequestEvent& operator-=(const Handler& handler) {
        for (auto it = mHandlers.begin(); it != mHandlers.end(); ++it) {
            auto f1 = (*it).target<handler_func(*)>();
            auto f2 = handler.target<handler_func>();

            if (!f1 || (f1 && f2 && *f1 == *f2)) {
                it = mHandlers.erase(it);
            }
        }
        
        return *this;
    }

    void operator()(const HttpServerRequest& req, HttpServerResponse& resp) {
        for (auto it : mHandlers) {
            it(req,resp);
        }
    }

private:
    std::list<Handler> mHandlers;
};

}
}

#endif

