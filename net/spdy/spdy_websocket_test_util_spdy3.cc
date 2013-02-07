// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_websocket_test_util_spdy3.h"

#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_test_util_spdy3.h"

static const int kDefaultAssociatedStreamId = 0;
static const bool kDefaultCompressed = false;
static const char* const kDefaultDataPointer = NULL;
static const uint32 kDefaultDataLength = 0;
static const char** const kDefaultExtraHeaders = NULL;
static const int kDefaultExtraHeaderCount = 0;
static const int kDefaultCredentialSlot = 0;

namespace net {

namespace test_spdy3 {

SpdyFrame* ConstructSpdyWebSocketSynStream(int stream_id,
                                           const char* path,
                                           const char* host,
                                           const char* origin) {
  const char* const kWebSocketHeaders[] = {
    ":path",
    path,
    ":host",
    host,
    ":version",
    "WebSocket/13",
    ":scheme",
    "ws",
    ":origin",
    origin
  };
  return ConstructSpdyControlFrame(/*extra_headers*/ NULL,
                                   /*extra_header_count*/ 0,
                                   /*compressed*/ false,
                                   stream_id,
                                   HIGHEST,
                                   SYN_STREAM,
                                   CONTROL_FLAG_NONE,
                                   kWebSocketHeaders,
                                   arraysize(kWebSocketHeaders));
}

SpdyFrame* ConstructSpdyWebSocketSynReply(int stream_id) {
  static const char* const kStandardWebSocketHeaders[] = {
    ":status",
    "101"
  };
  return ConstructSpdyControlFrame(NULL,
                                   0,
                                   false,
                                   stream_id,
                                   LOWEST,
                                   SYN_REPLY,
                                   CONTROL_FLAG_NONE,
                                   kStandardWebSocketHeaders,
                                   arraysize(kStandardWebSocketHeaders));
}

SpdyFrame* ConstructSpdyWebSocketHandshakeRequestFrame(
    const char* const headers[],
    int header_count,
    SpdyStreamId stream_id,
    RequestPriority request_priority) {

  // SPDY SYN_STREAM control frame header.
  const SpdyHeaderInfo kSynStreamHeader = {
    SYN_STREAM,
    stream_id,
    kDefaultAssociatedStreamId,
    ConvertRequestPriorityToSpdyPriority(request_priority, 3),
    kDefaultCredentialSlot,
    CONTROL_FLAG_NONE,
    kDefaultCompressed,
    INVALID,
    kDefaultDataPointer,
    kDefaultDataLength,
    DATA_FLAG_NONE
  };

  // Construct SPDY SYN_STREAM control frame.
  return ConstructSpdyPacket(
      kSynStreamHeader,
      kDefaultExtraHeaders,
      kDefaultExtraHeaderCount,
      headers,
      header_count);
}

SpdyFrame* ConstructSpdyWebSocketHandshakeResponseFrame(
    const char* const headers[],
    int header_count,
    SpdyStreamId stream_id,
    RequestPriority request_priority) {

  // SPDY SYN_REPLY control frame header.
  const SpdyHeaderInfo kSynReplyHeader = {
    SYN_REPLY,
    stream_id,
    kDefaultAssociatedStreamId,
    ConvertRequestPriorityToSpdyPriority(request_priority, 3),
    kDefaultCredentialSlot,
    CONTROL_FLAG_NONE,
    kDefaultCompressed,
    INVALID,
    kDefaultDataPointer,
    kDefaultDataLength,
    DATA_FLAG_NONE
  };

  // Construct SPDY SYN_REPLY control frame.
  return ConstructSpdyPacket(
      kSynReplyHeader,
      kDefaultExtraHeaders,
      kDefaultExtraHeaderCount,
      headers,
      header_count);
}

SpdyFrame* ConstructSpdyWebSocketHeadersFrame(int stream_id,
                                              const char* length,
                                              bool fin) {
  static const char* const kHeaders[] = {
    ":opcode",
    "1",  // text frame
    ":length",
    length,
    ":fin",
    fin ? "1" : "0"
  };
  return ConstructSpdyControlFrame(/*extra_headers*/ NULL,
                                   /*extra_header_count*/ 0,
                                   /*compression*/ false,
                                   stream_id,
                                   LOWEST,
                                   HEADERS,
                                   CONTROL_FLAG_NONE,
                                   kHeaders,
                                   arraysize(kHeaders));
}

SpdyFrame* ConstructSpdyWebSocketDataFrame(
    const char* data,
    int len,
    SpdyStreamId stream_id,
    bool fin) {

  // Construct SPDY data frame.
  BufferedSpdyFramer framer(3, false);
  return framer.CreateDataFrame(
      stream_id,
      data,
      len,
      fin ? DATA_FLAG_FIN : DATA_FLAG_NONE);
}

}  // namespace test_spdy3

}  // namespace net
