// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/time.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config_service.h"
#include "net/socket/client_socket_factory.h"

namespace net {

DnsSession::DnsSession(const DnsConfig& config,
                       ClientSocketFactory* factory,
                       const RandIntCallback& rand_int_callback,
                       NetLog* net_log)
    : config_(config),
      socket_factory_(factory),
      rand_callback_(base::Bind(rand_int_callback, 0, kuint16max)),
      net_log_(net_log),
      server_index_(0) {
}

int DnsSession::NextId() const {
  return rand_callback_.Run();
}

const IPEndPoint& DnsSession::NextServer() {
  // TODO(szym): Rotate servers on failures.
  const IPEndPoint& ipe = config_.nameservers[server_index_];
  if (config_.rotate)
    server_index_ = (server_index_ + 1) % config_.nameservers.size();
  return ipe;
}

base::TimeDelta DnsSession::NextTimeout(int attempt) {
  // TODO(szym): Adapt timeout to observed RTT.
  return config_.timeout * (attempt + 1);
}

DnsSession::~DnsSession() {}

}  // namespace net

