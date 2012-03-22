// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_WEBSOCKET_TEST_UTIL_H_
#define NET_SPDY_SPDY_WEBSOCKET_TEST_UTIL_H_
#pragma once

#include "net/base/request_priority.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

namespace test_spdy2 {

// Construct a WebSocket over SPDY handshake request packet.
SpdyFrame* ConstructSpdyWebSocketHandshakeRequestFrame(
    const char* const headers[],
    int header_count,
    SpdyStreamId stream_id,
    RequestPriority request_priority);

// Construct a WebSocket over SPDY handshake response packet.
SpdyFrame* ConstructSpdyWebSocketHandshakeResponseFrame(
    const char* const headers[],
    int header_count,
    SpdyStreamId stream_id,
    RequestPriority request_priority);

// Construct a WebSocket over SPDY data packet.
SpdyFrame* ConstructSpdyWebSocketDataFrame(
    const char* data,
    int len,
    SpdyStreamId stream_id,
    bool fin);

}  // namespace test_spdy2

}  // namespace net

#endif  // NET_SPDY_SPDY_WEBSOCKET_TEST_UTIL_H_
