// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CLIENT_H_
#define NET_DNS_DNS_CLIENT_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

struct DnsConfig;
class DnsTransactionFactory;
class NetLog;

// Convenience wrapper allows easy injection of DnsTransaction into
// HostResolverImpl.
class NET_EXPORT DnsClient {
 public:
  virtual ~DnsClient() {}

  // Creates a new DnsTransactionFactory according to the new |config|.
  virtual void SetConfig(const DnsConfig& config) = 0;

  // Returns NULL if the current config is not valid.
  virtual const DnsConfig* GetConfig() const = 0;

  // Returns NULL if the current config is not valid.
  virtual DnsTransactionFactory* GetTransactionFactory() = 0;

  // Creates default client.
  static scoped_ptr<DnsClient> CreateClient(NetLog* net_log);
};

}  // namespace net

#endif  // NET_DNS_DNS_CLIENT_H_

