// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_H_
#define NET_DNS_DNS_CONFIG_SERVICE_H_
#pragma once

#include <string>
#include <vector>

#include "base/time.h"
#include "net/base/net_export.h"

namespace net {

class IPEndPoint;

// DnsConfig stores configuration of the system resolver.
struct NET_EXPORT_PRIVATE DnsConfig {
  DnsConfig();
  virtual ~DnsConfig();

  bool Equals(const DnsConfig& d) const;

  bool Valid() const {
    return !nameservers.empty();
  }

  // List of name server addresses.
  std::vector<IPEndPoint> nameservers;
  // Suffix search list; used on first lookup when number of dots in given name
  // is less than |ndots|.
  std::vector<std::string> search;

  // Resolver options; see man resolv.conf.
  // TODO(szym): use |ndots| and |search| to determine the sequence of FQDNs
  // to query given a specific name.

  // Minimum number of dots before global resolution precedes |search|.
  int ndots;
  // Time between retransmissions, see res_state.retrans.
  base::TimeDelta timeout;
  // Maximum number of retries, see res_state.retry.
  int attempts;
  // Round robin entries in |nameservers| for subsequent requests.
  bool rotate;
  // Enable EDNS0 extensions.
  bool edns0;
};

// Service for watching when the system DNS settings have changed.
// Depending on the platform, watches files in /etc/ or win registry.
class NET_EXPORT_PRIVATE DnsConfigService {
 public:
  // Callback interface for the client. The observer is called on the same
  // thread as Watch(). Observer must outlive the service.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called only when |dns_config| is different from the last check.
    virtual void OnConfigChanged(const DnsConfig& dns_config) = 0;
  };

  // Creates the platform-specific DnsConfigService.
  static DnsConfigService* CreateSystemService();

  DnsConfigService() {}
  virtual ~DnsConfigService() {}

  // Immediately starts watching system configuration for changes and attempts
  // to read the configuration. For some platform implementations, the current
  // thread must have an IO loop (for base::files::FilePathWatcher).
  virtual void Watch() = 0;

  // If a config is available, |observer| will immediately be called with
  // OnConfigChanged.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DnsConfigService);
};

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_H_
