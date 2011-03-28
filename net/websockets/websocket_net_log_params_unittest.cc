// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/websockets/websocket_net_log_params.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NetLogWebSocketHandshakeParameterTest, ToValue) {
  ListValue* list = new ListValue();
  list->Append(new StringValue("GET /demo HTTP/1.1"));
  list->Append(new StringValue("Host: example.com"));
  list->Append(new StringValue("Connection: Upgrade"));
  list->Append(new StringValue("Sec-WebSocket-Key2: 12998 5 Y3 1  .P00"));
  list->Append(new StringValue("Sec-WebSocket-Protocol: sample"));
  list->Append(new StringValue("Upgrade: WebSocket"));
  list->Append(new StringValue("Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5"));
  list->Append(new StringValue("Origin: http://example.com"));
  list->Append(new StringValue(""));
  list->Append(new StringValue("\\x00\\x01\\x0a\\x0d\\xff\\xfe\\x0d\\x0a"));

  DictionaryValue expected;
  expected.Set("headers", list);

  const std::string key("\x00\x01\x0a\x0d\xff\xfe\x0d\x0a", 8);
  const std::string testInput =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Upgrade: WebSocket\r\n"
    "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
    "Origin: http://example.com\r\n"
    "\r\n" +
    key;

  scoped_refptr<net::NetLogWebSocketHandshakeParameter> parameter(
      new net::NetLogWebSocketHandshakeParameter(testInput));
  scoped_ptr<Value> actual(parameter->ToValue());

  EXPECT_TRUE(expected.Equals(actual.get()));
}
