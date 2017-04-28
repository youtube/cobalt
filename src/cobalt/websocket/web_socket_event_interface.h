// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_WEBSOCKET_WEB_SOCKET_EVENT_INTERFACE_H_
#define COBALT_WEBSOCKET_WEB_SOCKET_EVENT_INTERFACE_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/io_buffer.h"

namespace cobalt {
namespace websocket {

class WebsocketEventInterface {
 public:
  virtual ~WebsocketEventInterface() {}
  virtual void OnConnected(const std::string& selected_subprotocol) = 0;
  virtual void OnDisconnected(bool was_clean, uint16 code,
                              const std::string& reason) = 0;
  virtual void OnSentData(int amount_sent) = 0;
  virtual void OnReceivedData(bool is_text_frame,
                              scoped_refptr<net::IOBufferWithSize> data) = 0;
  virtual void OnError() = 0;

 protected:
  WebsocketEventInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebsocketEventInterface);
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_EVENT_INTERFACE_H_
