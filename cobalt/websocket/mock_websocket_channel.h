// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_MOCK_WEBSOCKET_CHANNEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_MOCK_WEBSOCKET_CHANNEL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "cobalt/network/network_module.h"
#include "cobalt/websocket/web_socket_impl.h"
#include "net/websockets/websocket_channel.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace websocket {

class SourceLocation;

class MockWebSocketChannel : public net::WebSocketChannel {
 public:
  MockWebSocketChannel(WebSocketImpl* impl,
                       network::NetworkModule* network_module);
  ~MockWebSocketChannel();

  MOCK_METHOD4(MockSendFrame,
               net::WebSocketChannel::ChannelState(
                   bool fin, net::WebSocketFrameHeader::OpCode op_code,
                   scoped_refptr<net::IOBuffer> buffer, size_t buffer_size));
  net::WebSocketChannel::ChannelState SendFrame(
      bool fin, net::WebSocketFrameHeader::OpCode op_code,
      scoped_refptr<net::IOBuffer> buffer, size_t buffer_size) override {
    base::AutoLock scoped_lock(lock_);
    return MockSendFrame(fin, op_code, buffer, buffer_size);
  }

  base::Lock& lock() { return lock_; }

 private:
  base::Lock lock_;
};

}  // namespace websocket
}  // namespace cobalt

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_MOCK_WEBSOCKET_CHANNEL_H_