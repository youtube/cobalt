// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_WEBSOCKET_WEB_SOCKET_MESSAGE_CONTAINER_H_
#define COBALT_WEBSOCKET_WEB_SOCKET_MESSAGE_CONTAINER_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "cobalt/websocket/web_socket_frame_container.h"
#include "net/base/io_buffer.h"
#include "net/websockets/websocket_frame.h"

namespace cobalt {
namespace websocket {

const size_t kMaxMessagePayloadInBytes = 4 * 1024 * 1024;
COMPILE_ASSERT(kMaxMessagePayloadInBytes >= kMaxFramePayloadInBytes,
               max_message_size_must_be_greater_than_max_payload_size);

class WebSocketMessageContainer {
 public:
  typedef std::deque<WebSocketFrameContainer> WebSocketFrames;

  WebSocketMessageContainer()
      : message_completed_(false), payload_size_bytes_(0) {}
  ~WebSocketMessageContainer() { clear(); }

  void clear() {
    message_completed_ = false;
    payload_size_bytes_ = 0;
    frames_.clear();
  }

  bool GetMessageOpCode(net::WebSocketFrameHeader::OpCode *op_code) const {
    DCHECK(op_code);
    if (empty()) {
      return false;
    }

    return frames_.begin()->GetFrameOpCode(op_code);
  }

  bool IsMessageComplete() const { return message_completed_; }

  // Returns true if and only if it a text message.
  // Note: It is valid to call this function on uncompleted messages.
  bool IsTextMessage() const {
    net::WebSocketFrameHeader::OpCode message_op_code =
        net::WebSocketFrameHeader::kOpCodeContinuation;

    bool success = GetMessageOpCode(&message_op_code);
    if (!success) {
      DLOG(INFO) << "Unable to retrieve the message op code.  Empty message?";
      return false;
    }

    DCHECK_NE(message_op_code, net::WebSocketFrameHeader::kOpCodePing);
    DCHECK_NE(message_op_code, net::WebSocketFrameHeader::kOpCodePong);
    DCHECK_NE(message_op_code, net::WebSocketFrameHeader::kOpCodeClose);

    return (message_op_code == net::WebSocketFrameHeader::kOpCodeText);
  }

  // Should only be called if IsMessageComplete() is false, and
  // |frame_container| is a full frame.
  bool Take(WebSocketFrameContainer *frame_container);

  std::size_t GetCurrentPayloadSizeBytes() const { return payload_size_bytes_; }

  scoped_refptr<net::IOBufferWithSize> GetMessageAsIOBuffer() const;

  const WebSocketFrames &GetFrames() const { return frames_; }
  bool empty() const { return frames_.empty(); }

 private:
  bool message_completed_;
  std::size_t payload_size_bytes_;
  WebSocketFrames frames_;
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_MESSAGE_CONTAINER_H_
