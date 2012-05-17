// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#pragma once

#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>

#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {

// Use DnsConfigService::CreateSystemService to use it outside of tests.
namespace internal {

class NET_EXPORT_PRIVATE DnsConfigServicePosix : public DnsConfigService {
 public:
  DnsConfigServicePosix();
  virtual ~DnsConfigServicePosix();

 private:
  // NetworkChangeNotifier::DNSObserver:
  virtual void OnDNSChanged(unsigned detail) OVERRIDE;

  scoped_refptr<SerialWorker> config_reader_;
  scoped_refptr<SerialWorker> hosts_reader_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServicePosix);
};

// Fills in |dns_config| from |res|.
bool NET_EXPORT_PRIVATE ConvertResStateToDnsConfig(
    const struct __res_state& res, DnsConfig* dns_config);

}  // namespace internal

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
