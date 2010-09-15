// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HTTP_UTILS_H_
#define NET_SPDY_SPDY_HTTP_UTILS_H_
#pragma once

#include "net/spdy/spdy_framer.h"

namespace net {

class HttpResponseInfo;
struct HttpRequestInfo;

// Convert a SpdyHeaderBlock into an HttpResponseInfo.
// |headers| input parameter with the SpdyHeaderBlock.
// |info| output parameter for the HttpResponseInfo.
// Returns true if successfully converted.  False if there was a failure
// or if the SpdyHeaderBlock was invalid.
bool SpdyHeadersToHttpResponse(const spdy::SpdyHeaderBlock& headers,
                               HttpResponseInfo* response);

// Create a SpdyHeaderBlock for a Spdy SYN_STREAM Frame from
// a HttpRequestInfo block.
void CreateSpdyHeadersFromHttpRequest(const HttpRequestInfo& info,
                                      spdy::SpdyHeaderBlock* headers,
                                      bool direct);

}  // namespace net

#endif  // NET_SPDY_SPDY_HTTP_UTILS_H_
