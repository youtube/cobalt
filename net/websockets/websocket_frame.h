// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_FRAME_H_
#define NET_WEBSOCKETS_WEBSOCKET_FRAME_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

// Represents a WebSocket frame header.
//
// Members of this class correspond to each element in WebSocket frame header
// (see http://tools.ietf.org/html/rfc6455#section-5.2).
struct NET_EXPORT_PRIVATE WebSocketFrameHeader {
  typedef int OpCode;
  static const OpCode kOpCodeContinuation;
  static const OpCode kOpCodeText;
  static const OpCode kOpCodeBinary;
  static const OpCode kOpCodeClose;
  static const OpCode kOpCodePing;
  static const OpCode kOpCodePong;

  // These values must be a compile-time constant. "enum hack" is used here
  // to make MSVC happy.
  enum {
    kBaseHeaderSize = 2,
    kMaximumExtendedLengthSize = 8,
    kMaskingKeyLength = 4
  };

  // Members below correspond to each item in WebSocket frame header.
  // See <http://tools.ietf.org/html/rfc6455#section-5.2> for details.
  bool final;
  bool reserved1;
  bool reserved2;
  bool reserved3;
  OpCode opcode;
  bool masked;
  uint64 payload_length;
};

// Contains payload data of part of a WebSocket frame.
//
// Payload of a WebSocket frame may be divided into multiple chunks.
// You need to look at |final_chunk| member variable to detect the end of a
// series of chunk objects of a WebSocket frame.
//
// Frame dissection is necessary to handle WebSocket frame stream containing
// abritrarily large frames in the browser process. Because the server may send
// a huge frame that doesn't fit in the memory, we cannot store the entire
// payload data in the memory.
//
// Users of this struct should treat WebSocket frames as a data stream; it's
// important to keep the frame data flowing, especially in the browser process.
// Users should not let the data stuck somewhere in the pipeline.
struct NET_EXPORT_PRIVATE WebSocketFrameChunk {
  WebSocketFrameChunk();
  ~WebSocketFrameChunk();

  // Non-null |header| is provided only if this chunk is the first part of
  // a series of chunks.
  scoped_ptr<WebSocketFrameHeader> header;

  // Indicates this part is the last chunk of a frame.
  bool final_chunk;

  // |data| is always unmasked even if the frame is masked.
  std::vector<char> data;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_FRAME_H_
