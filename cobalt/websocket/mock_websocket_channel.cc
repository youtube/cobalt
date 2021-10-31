// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/websocket/mock_websocket_channel.h"
#include "cobalt/websocket/cobalt_web_socket_event_handler.h"

// Generated constructors and destructors for GMock objects are very large. By
// putting them in a separate file we can speed up compile times.

namespace cobalt {
namespace websocket {

MockWebSocketChannel::MockWebSocketChannel(
    WebSocketImpl* impl, network::NetworkModule* network_module)
    : net::WebSocketChannel(std::unique_ptr<net::WebSocketEventInterface>(
                                new CobaltWebSocketEventHandler(impl)),
                            network_module->url_request_context()) {}

MockWebSocketChannel::~MockWebSocketChannel() = default;

}  // namespace websocket
}  // namespace cobalt