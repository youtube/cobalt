// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#pragma once

#include <resolv.h>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {

class NET_EXPORT_PRIVATE DnsConfigServicePosix
  : NON_EXPORTED_BASE(public DnsConfigService) {
 public:
  class DnsConfigReader;

  DnsConfigServicePosix();
  virtual ~DnsConfigServicePosix();

  virtual void Watch() OVERRIDE;

 private:
  scoped_refptr<WatchingFileReader> config_reader_;
  scoped_refptr<WatchingFileReader> hosts_reader_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServicePosix);
};

// Fills in |dns_config| from |res|. Exposed for tests.
bool NET_EXPORT_PRIVATE ConvertResToConfig(const struct __res_state& res,
                                           DnsConfig* dns_config);

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
