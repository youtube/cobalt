// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_SESSION_H_
#define NET_DNS_DNS_SESSION_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_config_service.h"

namespace net {

class ClientSocketFactory;
class NetLog;

// Session parameters and state shared between DNS transactions.
// Ref-counted so that DnsClient::Request can keep working in absence of
// DnsClient. A DnsSession must be recreated when DnsConfig changes.
class NET_EXPORT_PRIVATE DnsSession
    : NON_EXPORTED_BASE(public base::RefCounted<DnsSession>) {
 public:
  typedef base::Callback<int()> RandCallback;

  DnsSession(const DnsConfig& config,
             ClientSocketFactory* factory,
             const RandIntCallback& rand_int_callback,
             NetLog* net_log);

  ClientSocketFactory* socket_factory() const { return socket_factory_.get(); }
  const DnsConfig& config() const { return config_; }
  NetLog* net_log() const { return net_log_; }

  // Return the next random query ID.
  int NextId() const;

  // Return the next server address.
  const IPEndPoint& NextServer();

  // Return the timeout for the next transaction.
  base::TimeDelta NextTimeout(int attempt);

 private:
  friend class base::RefCounted<DnsSession>;
  ~DnsSession();

  const DnsConfig config_;
  scoped_ptr<ClientSocketFactory> socket_factory_;
  RandCallback rand_callback_;
  NetLog* net_log_;

  // Current index into |config_.nameservers|.
  int server_index_;

  // TODO(szym): add current RTT estimate
  // TODO(szym): add flag to indicate DNSSEC is supported
  // TODO(szym): add TCP connection pool to support DNS over TCP
  // TODO(szym): add UDP socket pool ?

  DISALLOW_COPY_AND_ASSIGN(DnsSession);
};

}  // namespace net

#endif  // NET_DNS_DNS_SESSION_H_

