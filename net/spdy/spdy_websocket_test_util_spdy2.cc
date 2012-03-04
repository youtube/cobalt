// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_websocket_test_util_spdy2.h"

#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_test_util_spdy2.h"

static const int kDefaultAssociatedStreamId = 0;
static const bool kDefaultCompressed = false;
static const char* const kDefaultDataPointer = NULL;
static const uint32 kDefaultDataLength = 0;
static const char** const kDefaultExtraHeaders = NULL;
static const int kDefaultExtraHeaderCount = 0;

namespace net {

namespace test_spdy2 {

spdy::SpdyFrame* ConstructSpdyWebSocketHandshakeRequestFrame(
    const char* const headers[],
    int header_count,
    spdy::SpdyStreamId stream_id,
    RequestPriority request_priority) {

  // SPDY SYN_STREAM control frame header.
  const SpdyHeaderInfo kSynStreamHeader = {
    spdy::SYN_STREAM,
    stream_id,
    kDefaultAssociatedStreamId,
    ConvertRequestPriorityToSpdyPriority(request_priority),
    spdy::CONTROL_FLAG_NONE,
    kDefaultCompressed,
    spdy::INVALID,
    kDefaultDataPointer,
    kDefaultDataLength,
    spdy::DATA_FLAG_NONE
  };

  // Construct SPDY SYN_STREAM control frame.
  return ConstructSpdyPacket(
      kSynStreamHeader,
      kDefaultExtraHeaders,
      kDefaultExtraHeaderCount,
      headers,
      header_count);
}

spdy::SpdyFrame* ConstructSpdyWebSocketHandshakeResponseFrame(
    const char* const headers[],
    int header_count,
    spdy::SpdyStreamId stream_id,
    RequestPriority request_priority) {

  // SPDY SYN_REPLY control frame header.
  const SpdyHeaderInfo kSynReplyHeader = {
    spdy::SYN_REPLY,
    stream_id,
    kDefaultAssociatedStreamId,
    ConvertRequestPriorityToSpdyPriority(request_priority),
    spdy::CONTROL_FLAG_NONE,
    kDefaultCompressed,
    spdy::INVALID,
    kDefaultDataPointer,
    kDefaultDataLength,
    spdy::DATA_FLAG_NONE
  };

  // Construct SPDY SYN_REPLY control frame.
  return ConstructSpdyPacket(
      kSynReplyHeader,
      kDefaultExtraHeaders,
      kDefaultExtraHeaderCount,
      headers,
      header_count);
}

spdy::SpdyFrame* ConstructSpdyWebSocketDataFrame(
    const char* data,
    int len,
    spdy::SpdyStreamId stream_id,
    bool fin) {

  // Construct SPDY data frame.
  spdy::SpdyFramer framer;
  return framer.CreateDataFrame(
      stream_id,
      data,
      len,
      fin ? spdy::DATA_FLAG_FIN : spdy::DATA_FLAG_NONE);
}

}  // namespace test_spdy2

}  // namespace net
