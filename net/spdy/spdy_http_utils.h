// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HTTP_UTILS_H_
#define NET_SPDY_SPDY_HTTP_UTILS_H_
#pragma once

#include "net/base/request_priority.h"
#include "net/spdy/spdy_framer.h"

namespace net {

class HttpResponseInfo;
struct HttpRequestInfo;
class HttpRequestHeaders;

// Convert a SpdyHeaderBlock into an HttpResponseInfo.
// |headers| input parameter with the SpdyHeaderBlock.
// |info| output parameter for the HttpResponseInfo.
// Returns true if successfully converted.  False if the SpdyHeaderBlock is
// incomplete (e.g. missing 'status' or 'version').
bool SpdyHeadersToHttpResponse(const spdy::SpdyHeaderBlock& headers,
                               HttpResponseInfo* response);

// Create a SpdyHeaderBlock for a Spdy SYN_STREAM Frame from
// HttpRequestInfo and HttpRequestHeaders.
void CreateSpdyHeadersFromHttpRequest(const HttpRequestInfo& info,
                                      const HttpRequestHeaders& request_headers,
                                      spdy::SpdyHeaderBlock* headers,
                                      bool direct);

int ConvertRequestPriorityToSpdyPriority(RequestPriority priority);

}  // namespace net

#endif  // NET_SPDY_SPDY_HTTP_UTILS_H_
