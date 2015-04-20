// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_ERRORS_H_
#define NET_WEBSOCKETS_WEBSOCKET_ERRORS_H_

#include "net/base/net_errors.h"

namespace net {

// Error values for WebSocket framing.
// This values are compatible to RFC6455 defined status codes.
enum WebSocketError {
  WEB_SOCKET_OK = 1000,
  WEB_SOCKET_ERR_PROTOCOL_ERROR = 1002,
  WEB_SOCKET_ERR_MESSAGE_TOO_BIG = 1009
};

// Convert WebSocketError to net::Error defined in net/base/net_errors.h.
Error WebSocketErrorToNetError(WebSocketError error);

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_FRAME_H_
