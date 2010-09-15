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
    std::vector<std::string> lines;
    base::SplitStringDontTrim(headers_, '\n', &lines);
    for (size_t i = 0; i < lines.size(); ++i) {
      if (lines[i] == "\r") {
        headers->Append(new StringValue(""));
        // Dump WebSocket key3
        std::string key;
        i = i + 1;
        for (; i < lines.size(); ++i) {
          for (size_t j = 0; j < lines[i].length(); ++j) {
            key += StringPrintf("\\x%02x", lines[i][j] & 0xff);
          }
          key += "\\x0a";
        }
        headers->Append(new StringValue(key));
        break;
      }
      std::string line = lines[i];
      if (line.length() > 0 && line[line.length() - 1] == '\r')
        line = line.substr(0, line.length() - 1);
      headers->Append(new StringValue(line));
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
