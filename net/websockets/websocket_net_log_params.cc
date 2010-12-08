// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_net_log_params.h"

namespace net {

NetLogWebSocketHandshakeParameter::NetLogWebSocketHandshakeParameter(
    const std::string& headers)
    : headers_(headers) {
}

Value* NetLogWebSocketHandshakeParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  ListValue* headers = new ListValue();

  size_t last = 0;
  size_t headers_size = headers_.size();
  size_t pos = 0;
  while (pos <= headers_size) {
    if (pos == headers_size ||
        (headers_[pos] == '\r' &&
         pos + 1 < headers_size && headers_[pos + 1] == '\n')) {
      std::string entry = headers_.substr(last, pos - last);
      pos += 2;
      last = pos;

      headers->Append(new StringValue(entry));

      if (entry.empty()) {
        // Dump WebSocket key3.
        std::string key;
        for (; pos < headers_size; ++pos) {
          key += base::StringPrintf("\\x%02x", headers_[pos] & 0xff);
        }
        headers->Append(new StringValue(key));
        break;
      }
    } else {
      ++pos;
    }
  }

  dict->Set("headers", headers);
  return dict;
}

NetLogWebSocketHandshakeParameter::~NetLogWebSocketHandshakeParameter() {}

}  // namespace net
