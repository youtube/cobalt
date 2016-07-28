// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_handler.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

namespace {

TEST(WebSocketHandshakeHandlerSpdy3Test, RequestResponse) {
  WebSocketHandshakeRequestHandler request_handler;

  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Extensions: foo\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "X-Foo: foo\r\n"
      "\r\n";

  EXPECT_TRUE(request_handler.ParseRequest(kHandshakeRequestMessage,
                                           strlen(kHandshakeRequestMessage)));
  EXPECT_EQ(13, request_handler.protocol_version());

  GURL url("ws://example.com/demo");
  std::string challenge;
  SpdyHeaderBlock headers;
  ASSERT_TRUE(request_handler.GetRequestHeaderBlock(url,
                                                    &headers,
                                                    &challenge,
                                                    3));

  EXPECT_EQ(url.path(), headers[":path"]);
  EXPECT_TRUE(headers.find(":upgrade") == headers.end());
  EXPECT_TRUE(headers.find(":Upgrade") == headers.end());
  EXPECT_TRUE(headers.find(":connection") == headers.end());
  EXPECT_TRUE(headers.find(":Connection") == headers.end());
  EXPECT_TRUE(headers.find(":Sec-WebSocket-Key") == headers.end());
  EXPECT_TRUE(headers.find(":sec-websocket-key") == headers.end());
  EXPECT_TRUE(headers.find(":Sec-WebSocket-Version") == headers.end());
  EXPECT_TRUE(headers.find(":sec-webSocket-version") == headers.end());
  EXPECT_TRUE(headers.find(":x-foo") == headers.end());
  EXPECT_EQ("example.com", headers[":host"]);
  EXPECT_EQ("http://example.com", headers[":origin"]);
  EXPECT_EQ("sample", headers[":sec-websocket-protocol"]);
  EXPECT_EQ("foo", headers[":sec-websocket-extensions"]);
  EXPECT_EQ("ws", headers[":scheme"]);
  EXPECT_EQ("WebSocket/13", headers[":version"]);
  EXPECT_EQ("foo", headers["x-foo"]);

  static const char expected_challenge[] = "dGhlIHNhbXBsZSBub25jZQ==";

  EXPECT_EQ(expected_challenge, challenge);

  headers.clear();

  headers[":status"] = "101 Switching Protocols";
  headers[":sec-websocket-protocol"] = "sample";
  headers[":sec-websocket-extensions"] = "foo";
  headers["x-bar"] = "bar";

  WebSocketHandshakeResponseHandler response_handler;
  response_handler.set_protocol_version(13);
  EXPECT_TRUE(response_handler.ParseResponseHeaderBlock(headers,
                                                        challenge,
                                                        3));
  EXPECT_TRUE(response_handler.HasResponse());

  // Note that order of sec-websocket-* is sensitive with hash_map order.
  static const char kHandshakeResponseExpectedMessage[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "sec-websocket-extensions: foo\r\n"
      "sec-websocket-protocol: sample\r\n"
      "x-bar: bar\r\n"
      "\r\n";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, response_handler.GetResponse());
}

TEST(WebSocketHandshakeHandlerSpdy3Test, RequestResponseWithCookies) {
  WebSocketHandshakeRequestHandler request_handler;

  // Note that websocket won't use multiple headers in request now.
  static const char kHandshakeRequestMessage[] =
      "GET /demo HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Origin: http://example.com\r\n"
      "Sec-WebSocket-Protocol: sample\r\n"
      "Sec-WebSocket-Extensions: foo\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "Cookie: WK-websocket-test=1; WK-websocket-test-httponly=1\r\n"
      "\r\n";

  EXPECT_TRUE(request_handler.ParseRequest(kHandshakeRequestMessage,
                                           strlen(kHandshakeRequestMessage)));
  EXPECT_EQ(13, request_handler.protocol_version());

  GURL url("ws://example.com/demo");
  std::string challenge;
  SpdyHeaderBlock headers;
  ASSERT_TRUE(request_handler.GetRequestHeaderBlock(url,
                                                    &headers,
                                                    &challenge,
                                                    3));

  EXPECT_EQ(url.path(), headers[":path"]);
  EXPECT_TRUE(headers.find(":upgrade") == headers.end());
  EXPECT_TRUE(headers.find(":Upgrade") == headers.end());
  EXPECT_TRUE(headers.find(":connection") == headers.end());
  EXPECT_TRUE(headers.find(":Connection") == headers.end());
  EXPECT_TRUE(headers.find(":Sec-WebSocket-Key") == headers.end());
  EXPECT_TRUE(headers.find(":sec-websocket-key") == headers.end());
  EXPECT_TRUE(headers.find(":Sec-WebSocket-Version") == headers.end());
  EXPECT_TRUE(headers.find(":sec-webSocket-version") == headers.end());
  EXPECT_TRUE(headers.find(":Cookie") == headers.end());
  EXPECT_TRUE(headers.find(":cookie") == headers.end());
  EXPECT_EQ("example.com", headers[":host"]);
  EXPECT_EQ("http://example.com", headers[":origin"]);
  EXPECT_EQ("sample", headers[":sec-websocket-protocol"]);
  EXPECT_EQ("foo", headers[":sec-websocket-extensions"]);
  EXPECT_EQ("ws", headers[":scheme"]);
  EXPECT_EQ("WebSocket/13", headers[":version"]);
  EXPECT_EQ("WK-websocket-test=1; WK-websocket-test-httponly=1",
            headers["cookie"]);

  const char expected_challenge[] = "dGhlIHNhbXBsZSBub25jZQ==";

  EXPECT_EQ(expected_challenge, challenge);

  headers.clear();

  headers[":status"] = "101 Switching Protocols";
  headers[":sec-websocket-protocol"] = "sample";
  headers[":sec-websocket-extensions"] = "foo";
  std::string cookie = "WK-websocket-test=1";
  cookie.append(1, '\0');
  cookie += "WK-websocket-test-httponly=1; HttpOnly";
  headers["set-cookie"] = cookie;


  WebSocketHandshakeResponseHandler response_handler;
  response_handler.set_protocol_version(13);
  EXPECT_TRUE(response_handler.ParseResponseHeaderBlock(headers,
                                                        challenge,
                                                        3));
  EXPECT_TRUE(response_handler.HasResponse());

  // Note that order of sec-websocket-* is sensitive with hash_map order.
  static const char kHandshakeResponseExpectedMessage[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "sec-websocket-extensions: foo\r\n"
      "sec-websocket-protocol: sample\r\n"
      "set-cookie: WK-websocket-test=1\r\n"
      "set-cookie: WK-websocket-test-httponly=1; HttpOnly\r\n"
      "\r\n";

  EXPECT_EQ(kHandshakeResponseExpectedMessage, response_handler.GetResponse());
}

}  // namespace

}  // namespace net
