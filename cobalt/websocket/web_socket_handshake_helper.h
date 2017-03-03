/* Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_WEBSOCKET_WEB_SOCKET_HANDSHAKE_HELPER_H_
#define COBALT_WEBSOCKET_WEB_SOCKET_HANDSHAKE_HELPER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/string_piece.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"
#include "net/websockets/websocket_frame.h"

namespace cobalt {
namespace websocket {

class WebSocketHandshakeHelper {
 public:
  typedef net::WebSocketMaskingKey (*WebSocketMaskingKeyGeneratorFunction)();

  WebSocketHandshakeHelper();

  // Overriding the key-generation function is useful for testing.
  explicit WebSocketHandshakeHelper(
      WebSocketMaskingKeyGeneratorFunction key_generator_function);

  void GenerateHandshakeRequest(
      const GURL& connect_url, const std::string& origin,
      const std::vector<std::string>& desired_sub_protocols,
      std::string* handshake_request);

  bool IsResponseValid(const net::HttpResponseHeaders& headers,
                       std::string* failure_message);

  const std::string& GetSelectedSubProtocol() const {
    return selected_subprotocol_;
  }

 private:
  // Having key generator function passed is slightly slower, but very useful
  // for testing.
  WebSocketMaskingKeyGeneratorFunction key_generator_function_;

  std::string handshake_challenge_response_;
  net::WebSocketMaskingKey sec_websocket_key_;
  std::vector<std::string> requested_sub_protocols_;
  std::string selected_subprotocol_;

  void GenerateSecWebSocketKey();

  FRIEND_TEST_ALL_PREFIXES(WebSocketHandshakeHelperTest, null_key);

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeHelper);
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_WEB_SOCKET_HANDSHAKE_HELPER_H_
