// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_ERROR_PARAMS_H_
#define NET_SOCKET_SSL_ERROR_PARAMS_H_
#pragma once

#include "net/base/net_log.h"

namespace net {

// Extra parameters to attach to the NetLog when we receive an SSL error.
class SSLErrorParams : public NetLog::EventParameters {
 public:
  SSLErrorParams(int net_error, int ssl_lib_error);
  virtual ~SSLErrorParams();

  virtual Value* ToValue() const;

 private:
  const int net_error_;
  const int ssl_lib_error_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_ERROR_PARAMS_H_
