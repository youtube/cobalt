// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_constants.h"

namespace net {
namespace websockets {

const char kHttpProtocolVersion[] = "HTTP/1.1";

const size_t kRawChallengeLength = 16;

const char kSecWebSocketProtocol[] = "Sec-WebSocket-Protocol";
const char kSecWebSocketExtensions[] = "Sec-WebSocket-Extensions";
const char kSecWebSocketKey[] = "Sec-WebSocket-Key";
const char kSecWebSocketAccept[] = "Sec-WebSocket-Accept";
const char kSecWebSocketVersion[] = "Sec-WebSocket-Version";

const char kSupportedVersion[] = "13";

const char kUpgrade[] = "Upgrade";
const char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

const char kSecWebSocketProtocolSpdy3[] = ":sec-websocket-protocol";
const char kSecWebSocketExtensionsSpdy3[] = ":sec-websocket-extensions";

const char* const kSecWebSocketProtocolLowercase =
    kSecWebSocketProtocolSpdy3 + 1;
const char* const kSecWebSocketExtensionsLowercase =
    kSecWebSocketExtensionsSpdy3 + 1;
const char kSecWebSocketKeyLowercase[] = "sec-websocket-key";
const char kSecWebSocketVersionLowercase[] = "sec-websocket-version";
const char kUpgradeLowercase[] = "upgrade";
const char kWebSocketLowercase[] = "websocket";

}  // namespace websockets
}  // namespace net
