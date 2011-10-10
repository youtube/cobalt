// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_WIN_H_
#define NET_DNS_DNS_CONFIG_SERVICE_WIN_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {

class WatchingFileReader;

class NET_EXPORT_PRIVATE DnsConfigServiceWin
  : NON_EXPORTED_BASE(public DnsConfigService) {
 public:
  DnsConfigServiceWin();
  virtual ~DnsConfigServiceWin();

  virtual void Watch() OVERRIDE;

 private:
  class ConfigReader;
  scoped_refptr<ConfigReader> config_reader_;
  scoped_ptr<WatchingFileReader> hosts_watcher_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServiceWin);
};

// Parses |value| as search list (comma-delimited list of domain names) from
// a registry key and stores it in |out|. Returns true on success.
// Empty entries (e.g., "chromium.org,,org") terminate the list.
// Non-ascii hostnames are converted to punycode.
bool NET_EXPORT_PRIVATE ParseSearchList(const string16& value,
                                        std::vector<std::string>* out);

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_WIN_H_

