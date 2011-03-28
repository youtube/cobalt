// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_
#define NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "net/base/net_log.h"

namespace net {

class NetLogWebSocketHandshakeParameter : public NetLog::EventParameters {
 public:
  explicit NetLogWebSocketHandshakeParameter(const std::string& headers);

  virtual Value* ToValue() const;

 private:
  virtual ~NetLogWebSocketHandshakeParameter();

  const std::string headers_;

  DISALLOW_COPY_AND_ASSIGN(NetLogWebSocketHandshakeParameter);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_NET_LOG_PARAMS_H_
