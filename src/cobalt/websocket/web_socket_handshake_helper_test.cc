// Copyright 2013 The Chromium Authors. All rights reserved.
/* Modifications: Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/websocket/web_socket_handshake_helper.h"

#include "base/strings/string_piece.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/websockets/websocket_frame.h"
#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using cobalt::websocket::SecWebSocketKey;

SecWebSocketKey NullMaskingKeyGenerator() {
  SecWebSocketKey::SecWebSocketKeyBytes key_bytes;
  memset(&key_bytes, 0, sizeof(key_bytes));
  return SecWebSocketKey(key_bytes);
}

std::vector<net::HttpRequestHeaders::HeaderKeyValuePair> RequestHeadersToVector(
    const net::HttpRequestHeaders &headers) {
  net::HttpRequestHeaders::Iterator it(headers);
  std::vector<net::HttpRequestHeaders::HeaderKeyValuePair> result;
  while (it.GetNext())
    result.push_back(
        net::HttpRequestHeaders::HeaderKeyValuePair(it.name(), it.value()));
  return result;
}

const char kTestUserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) Cobalt/9.27875-debug (unlike Gecko) "
    "Starboard/2";

}  // namespace

namespace net {

bool operator==(const HttpRequestHeaders::HeaderKeyValuePair &lhs,
                const HttpRequestHeaders::HeaderKeyValuePair &rhs) {
  return (lhs.key == rhs.key) && (lhs.value == rhs.value);
}

std::ostream &operator<<(std::ostream &os,
                         const HttpRequestHeaders::HeaderKeyValuePair &obj) {
  os << obj.key << ":" << obj.value;
  return os;
}

}  // namespace net

namespace cobalt {
namespace websocket {

class WebSocketHandshakeHelperTest : public ::testing::Test {
 public:
  WebSocketHandshakeHelperTest()
      : handshake_helper_(kTestUserAgent, &NullMaskingKeyGenerator) {}

  WebSocketHandshakeHelper handshake_helper_;
  std::vector<std::string> sub_protocols_;
};

TEST_F(WebSocketHandshakeHelperTest, null_key) {
  EXPECT_EQ(static_cast<int>(SecWebSocketKey::kKeySizeInBytes), 16);

  handshake_helper_.GenerateSecWebSocketKey();
  std::string null_key(SecWebSocketKey::kKeySizeInBytes, '\0');
  EXPECT_EQ(
      memcmp(null_key.data(),
             handshake_helper_.sec_websocket_key_.GetRawKeyBytes(),
             SecWebSocketKey::kKeySizeInBytes),
      0);
}

TEST_F(WebSocketHandshakeHelperTest, HandshakeInfo) {
  std::string handshake_request;
  GURL localhost_websocket_endpoint("ws://localhost/");
  handshake_helper_.GenerateHandshakeRequest(localhost_websocket_endpoint,
                                             "http://localhost", sub_protocols_,
                                             &handshake_request);

  char end_of_line[] = "\r\n";
  std::size_t first_line_end = handshake_request.find(end_of_line);
  ASSERT_NE(first_line_end, std::string::npos);
  net::HttpRequestHeaders headers;
  base::StringPiece handshake_request_stringpiece(
      handshake_request.data() + first_line_end + sizeof(end_of_line) - 1);
  headers.AddHeadersFromString(handshake_request_stringpiece);
  std::vector<net::HttpRequestHeaders::HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(headers);

  typedef net::HttpRequestHeaders::HeaderKeyValuePair HeaderKeyValuePair;
  ASSERT_EQ(10u, request_headers.size());
  EXPECT_EQ(HeaderKeyValuePair("Host", "localhost"), request_headers[0]);
  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), request_headers[1]);
  EXPECT_EQ(HeaderKeyValuePair("Pragma", "no-cache"), request_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("Cache-Control", "no-cache"),
            request_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), request_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Extensions", ""),
            request_headers[5]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Version", "13"),
            request_headers[6]);
  EXPECT_EQ(HeaderKeyValuePair("Origin", "http://localhost"),
            request_headers[7]);
  EXPECT_EQ("Sec-WebSocket-Key", request_headers[8].key);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", kTestUserAgent),
            request_headers[9]);
}

TEST_F(WebSocketHandshakeHelperTest, HandshakeWithPort) {
  std::string handshake_request;
  GURL localhost_websocket_endpoint("ws://localhost:4541/");
  handshake_helper_.GenerateHandshakeRequest(localhost_websocket_endpoint,
                                             "http://localhost", sub_protocols_,
                                             &handshake_request);

  char end_of_line[] = "\r\n";
  std::size_t first_line_end = handshake_request.find(end_of_line);
  ASSERT_NE(first_line_end, std::string::npos);
  net::HttpRequestHeaders headers;
  base::StringPiece handshake_request_stringpiece(
      handshake_request.data() + first_line_end + sizeof(end_of_line) - 1);
  headers.AddHeadersFromString(handshake_request_stringpiece);
  std::vector<net::HttpRequestHeaders::HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(headers);

  typedef net::HttpRequestHeaders::HeaderKeyValuePair HeaderKeyValuePair;
  ASSERT_EQ(10u, request_headers.size());
  EXPECT_EQ(HeaderKeyValuePair("Host", "localhost:4541"), request_headers[0]);
  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), request_headers[1]);
  EXPECT_EQ(HeaderKeyValuePair("Pragma", "no-cache"), request_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("Cache-Control", "no-cache"),
            request_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), request_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Extensions", ""),
            request_headers[5]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Version", "13"),
            request_headers[6]);
  EXPECT_EQ(HeaderKeyValuePair("Origin", "http://localhost"),
            request_headers[7]);
  EXPECT_EQ("Sec-WebSocket-Key", request_headers[8].key);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", kTestUserAgent),
            request_headers[9]);
}

TEST_F(WebSocketHandshakeHelperTest, HandshakePath) {
  std::string handshake_request;
  GURL localhost_websocket_endpoint("ws://localhost:4541/abc-def");
  handshake_helper_.GenerateHandshakeRequest(localhost_websocket_endpoint,
                                             "http://localhost", sub_protocols_,
                                             &handshake_request);

  char end_of_line[] = "\r\n";
  std::size_t first_line_end = handshake_request.find(end_of_line);
  ASSERT_NE(first_line_end, std::string::npos);

  std::string http_GET_line = handshake_request.substr(0, first_line_end);
  EXPECT_EQ(http_GET_line, "GET /abc-def HTTP/1.1");
  net::HttpRequestHeaders headers;
  base::StringPiece handshake_request_stringpiece(
      handshake_request.data() + first_line_end + sizeof(end_of_line) - 1);
  headers.AddHeadersFromString(handshake_request_stringpiece);
  std::vector<net::HttpRequestHeaders::HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(headers);

  typedef net::HttpRequestHeaders::HeaderKeyValuePair HeaderKeyValuePair;
  ASSERT_EQ(10u, request_headers.size());
  EXPECT_EQ(HeaderKeyValuePair("Host", "localhost:4541"), request_headers[0]);
  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), request_headers[1]);
  EXPECT_EQ(HeaderKeyValuePair("Pragma", "no-cache"), request_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("Cache-Control", "no-cache"),
            request_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), request_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Extensions", ""),
            request_headers[5]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Version", "13"),
            request_headers[6]);
  EXPECT_EQ(HeaderKeyValuePair("Origin", "http://localhost"),
            request_headers[7]);
  EXPECT_EQ("Sec-WebSocket-Key", request_headers[8].key);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", kTestUserAgent),
            request_headers[9]);
}

TEST_F(WebSocketHandshakeHelperTest, HandshakePathWithQuery) {
  std::string handshake_request;
  GURL localhost_websocket_endpoint("ws://localhost:4541/abc?one=1&two=2");
  handshake_helper_.GenerateHandshakeRequest(localhost_websocket_endpoint,
                                             "http://localhost", sub_protocols_,
                                             &handshake_request);

  char end_of_line[] = "\r\n";
  std::size_t first_line_end = handshake_request.find(end_of_line);
  ASSERT_NE(first_line_end, std::string::npos);

  std::string http_GET_line = handshake_request.substr(0, first_line_end);
  EXPECT_EQ(http_GET_line, "GET /abc?one=1&two=2 HTTP/1.1");
  net::HttpRequestHeaders headers;
  base::StringPiece handshake_request_stringpiece(
      handshake_request.data() + first_line_end + sizeof(end_of_line) - 1);
  headers.AddHeadersFromString(handshake_request_stringpiece);
  std::vector<net::HttpRequestHeaders::HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(headers);

  typedef net::HttpRequestHeaders::HeaderKeyValuePair HeaderKeyValuePair;
  ASSERT_EQ(10u, request_headers.size());
  EXPECT_EQ(HeaderKeyValuePair("Host", "localhost:4541"), request_headers[0]);
  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), request_headers[1]);
  EXPECT_EQ(HeaderKeyValuePair("Pragma", "no-cache"), request_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("Cache-Control", "no-cache"),
            request_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), request_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Extensions", ""),
            request_headers[5]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Version", "13"),
            request_headers[6]);
  EXPECT_EQ(HeaderKeyValuePair("Origin", "http://localhost"),
            request_headers[7]);
  EXPECT_EQ("Sec-WebSocket-Key", request_headers[8].key);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", kTestUserAgent),
            request_headers[9]);
}

TEST_F(WebSocketHandshakeHelperTest, HandshakePathWithDesiredProtocols) {
  sub_protocols_.push_back("chat");
  sub_protocols_.push_back("superchat");

  std::string handshake_request;
  GURL localhost_websocket_endpoint("ws://localhost/");
  handshake_helper_.GenerateHandshakeRequest(localhost_websocket_endpoint,
                                             "http://localhost", sub_protocols_,
                                             &handshake_request);

  char end_of_line[] = "\r\n";
  std::size_t first_line_end = handshake_request.find(end_of_line);
  ASSERT_NE(first_line_end, std::string::npos);

  std::string http_GET_line = handshake_request.substr(0, first_line_end);
  EXPECT_EQ(http_GET_line, "GET / HTTP/1.1");
  net::HttpRequestHeaders headers;
  base::StringPiece handshake_request_stringpiece(
      handshake_request.data() + first_line_end + sizeof(end_of_line) - 1);
  headers.AddHeadersFromString(handshake_request_stringpiece);
  std::vector<net::HttpRequestHeaders::HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(headers);

  typedef net::HttpRequestHeaders::HeaderKeyValuePair HeaderKeyValuePair;
  ASSERT_EQ(11u, request_headers.size());
  EXPECT_EQ(HeaderKeyValuePair("Host", "localhost"), request_headers[0]);
  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), request_headers[1]);
  EXPECT_EQ(HeaderKeyValuePair("Pragma", "no-cache"), request_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("Cache-Control", "no-cache"),
            request_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), request_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Extensions", ""),
            request_headers[5]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Version", "13"),
            request_headers[6]);
  EXPECT_EQ(HeaderKeyValuePair("Origin", "http://localhost"),
            request_headers[7]);
  EXPECT_EQ("Sec-WebSocket-Key", request_headers[8].key);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", kTestUserAgent),
            request_headers[9]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Protocol", "chat,superchat"),
            request_headers[10]);
}

TEST_F(WebSocketHandshakeHelperTest, CheckValidResponseCode) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n"
      "\r\n";

  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_TRUE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(error, "");
}

TEST_F(WebSocketHandshakeHelperTest, CheckInValidResponseCode) {
  char response_on_wire[] =
      "HTTP/1.1 200 OK\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: Ei4axqShP74o6nOtmL3e5uRpei8=\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_EQ(error, "Invalid response code 200");
}

TEST_F(WebSocketHandshakeHelperTest, CheckValidUpgradeHeader) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_TRUE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(WebSocketHandshakeHelperTest, CheckMissingUpgradeHeader) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_EQ(error, "'Upgrade' header is missing");
}

TEST_F(WebSocketHandshakeHelperTest, CheckMultipleUpgradeHeaders) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Upgrade: HyperSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_EQ(error,
            "'Upgrade' header must not appear more than once in a response");
}

TEST_F(WebSocketHandshakeHelperTest, CheckValidSecWebSocketAccept) {
  // To verify Sec-WebSocket-Accept values for testing, python was used.
  //  In [1]: import struct
  //  In [2]: import hashlib
  //  In [3]: import base64
  //  In [5]: h = hashlib.sha1(base64.b64encode(struct.pack('QQ', 0, 0)) +
  //  b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11")
  //  In [6]: base64.b64encode(h.digest())
  //  Out[6]: 'ICX+Yqv66kxgM0FcWaLWlFLwTAI='

  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:ICX+Yqv66kxgM0FcWaLWlFLwTAI=\r\n";

  std::string handshake_request;
  GURL localhost_websocket_endpoint("ws://localhost:4541/abc?one=1&two=2");
  handshake_helper_.GenerateHandshakeRequest(localhost_websocket_endpoint,
                                             "http://localhost", sub_protocols_,
                                             &handshake_request);

  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_TRUE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(error, "");
}

TEST_F(WebSocketHandshakeHelperTest, CheckMissingSecWebSocketAccept) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_EQ(error, "'Sec-WebSocket-Accept' header is missing");
}

TEST_F(WebSocketHandshakeHelperTest, CheckMultipleSecWebSocketAccept) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n"
      "Sec-WebSocket-Accept:\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_EQ(error,
            "'Sec-WebSocket-Accept' header must not appear more than once in a "
            "response");
}

TEST_F(WebSocketHandshakeHelperTest, CheckValidConnection) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_TRUE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(WebSocketHandshakeHelperTest, CheckMissingConnectionHeader) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Sec-WebSocket-Accept:\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_EQ(error, "'Connection' header is missing");
}

TEST_F(WebSocketHandshakeHelperTest, CheckValidInvalidSubprotocol) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n"
      "Sec-WebSocket-Protocol:chat\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(error,
            "Response must not include 'Sec-WebSocket-Protocol' header if not "
            "present in request: chat");
}

TEST_F(WebSocketHandshakeHelperTest, CheckValidInvalidDifferentSubprotocol) {
  sub_protocols_.push_back("superchat");

  std::string handshake_request;
  GURL localhost_websocket_endpoint("ws://localhost/");
  handshake_helper_.GenerateHandshakeRequest(localhost_websocket_endpoint,
                                             "http://localhost", sub_protocols_,
                                             &handshake_request);
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n"
      "Sec-WebSocket-Protocol:chat\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(error, "Incorrect 'Sec-WebSocket-Accept' header value");
}

TEST_F(WebSocketHandshakeHelperTest, CheckSubprotocolHeaderIsOptional) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_TRUE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(WebSocketHandshakeHelperTest, CheckValidExtensionHeader) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n"
      "Sec-WebSocket-Extensions:mux\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_EQ(error, "Cobalt does not support any websocket extensions");
}

TEST_F(WebSocketHandshakeHelperTest, CheckExtensionHeaderIsOptional) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_TRUE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(WebSocketHandshakeHelperTest, CheckMultipleExtensionHeadersAreOK) {
  char response_on_wire[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept:\r\n"
      "Sec-WebSocket-Extensions:mux\r\n"
      "Sec-WebSocket-Extensions:demux\r\n";
  std::string transformed_headers = net::HttpUtil::AssembleRawHeaders(
      response_on_wire, arraysize(response_on_wire));
  scoped_refptr<net::HttpResponseHeaders> responseHeaders =
      new net::HttpResponseHeaders(transformed_headers);
  std::string error;
  EXPECT_FALSE(handshake_helper_.IsResponseValid(*responseHeaders, &error));
  EXPECT_EQ(error, "Cobalt does not support any websocket extensions");
}

}  // namespace websocket
}  // namespace cobalt
