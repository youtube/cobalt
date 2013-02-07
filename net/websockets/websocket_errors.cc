// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websocket/websocket_errors.h"

namespace net {

Error WebSocketErrorToNetError(WebSocketError error) {
  switch (error) {
    case WEB_SOCKET_OK:
      return OK;
    case WEB_SOCKET_ERR_PROTOCOL_ERROR:
      return ERR_WS_PROTOCOL_ERROR;
    case WEB_SOCKET_ERR_MESSAGE_TOO_BIG:
      return ERR_MSG_TOO_BIG;
    default:
      NOTREACHED();
  }
}

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_FRAME_H_
