// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_HISTOGRAMS_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_HISTOGRAMS_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/time.h"

namespace base {
class Histogram;
}

namespace net {

class ClientSocketPoolHistograms {
 public:
  ClientSocketPoolHistograms(const std::string& pool_name);
  ~ClientSocketPoolHistograms();

  void AddSocketType(int socket_reuse_type) const;
  void AddRequestTime(base::TimeDelta time) const;
  void AddUnusedIdleTime(base::TimeDelta time) const;
  void AddReusedIdleTime(base::TimeDelta time) const;

 private:
  base::Histogram* socket_type_;
  base::Histogram* request_time_;
  base::Histogram* unused_idle_time_;
  base::Histogram* reused_idle_time_;

  bool is_http_proxy_connection_;
  bool is_socks_connection_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolHistograms);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_HISTOGRAMS_H_
