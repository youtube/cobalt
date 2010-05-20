// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_HISTOGRAMS_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_HISTOGRAMS_H_

#include <string>

#include "base/histogram.h"
#include "base/ref_counted.h"

namespace net {

class ClientSocketPoolHistograms
    : public base::RefCounted<ClientSocketPoolHistograms> {
 public:
  ClientSocketPoolHistograms(const std::string& pool_name);

  void AddSocketType(int socket_reuse_type) const;
  void AddRequestTime(base::TimeDelta time) const;
  void AddUnusedIdleTime(base::TimeDelta time) const;
  void AddReusedIdleTime(base::TimeDelta time) const;

 private:
  friend class base::RefCounted<ClientSocketPoolHistograms>;
  ~ClientSocketPoolHistograms() {}

  scoped_refptr<Histogram> socket_type_;
  scoped_refptr<Histogram> request_time_;
  scoped_refptr<Histogram> unused_idle_time_;
  scoped_refptr<Histogram> reused_idle_time_;
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_HISTOGRAMS_H_
