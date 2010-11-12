// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_
#define NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "net/base/net_log.h"

namespace net {

class NetLogWebSocketHandshakeParameter : public NetLog::EventParameters {
 public:
  explicit NetLogWebSocketHandshakeParameter(const std::string& headers)
      : headers_(headers) {
  }

  Value* ToValue() const {
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

 private:
  ~NetLogWebSocketHandshakeParameter() {}

  const std::string headers_;

  DISALLOW_COPY_AND_ASSIGN(NetLogWebSocketHandshakeParameter);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_
