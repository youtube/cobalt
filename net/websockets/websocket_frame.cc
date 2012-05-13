// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame.h"

namespace net {

// Definitions for in-struct constants.
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodeContinuation =
    0x0;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodeText = 0x1;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodeBinary = 0x2;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodeClose = 0x8;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodePing = 0x9;
const WebSocketFrameHeader::OpCode WebSocketFrameHeader::kOpCodePong = 0xA;

WebSocketFrameChunk::WebSocketFrameChunk() : final_chunk(false) {
}

WebSocketFrameChunk::~WebSocketFrameChunk() {
}

}  // namespace net
