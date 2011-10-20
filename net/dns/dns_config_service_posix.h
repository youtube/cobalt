// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#pragma once

#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {

class WatchingFileReader;

class NET_EXPORT_PRIVATE DnsConfigServicePosix
  : NON_EXPORTED_BASE(public DnsConfigService) {
 public:
  DnsConfigServicePosix();
  virtual ~DnsConfigServicePosix();

  virtual void Watch() OVERRIDE;

 private:
  class ConfigReader;
  scoped_ptr<WatchingFileReader> config_watcher_;
  scoped_ptr<WatchingFileReader> hosts_watcher_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServicePosix);
};

// Fills in |dns_config| from |res|. Exposed for tests.
bool NET_EXPORT_PRIVATE ConvertResToConfig(const struct __res_state& res,
                                           DnsConfig* dns_config);

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
