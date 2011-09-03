// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_H_
#define NET_DNS_DNS_CONFIG_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "net/base/ip_endpoint.h"  // win requires size of IPEndPoint
#include "net/base/net_export.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/watching_file_reader.h"

namespace net {

// DnsConfig stores configuration of the system resolver.
struct NET_EXPORT_PRIVATE DnsConfig {
  DnsConfig();
  virtual ~DnsConfig();

  bool Equals(const DnsConfig& d) const;

  bool EqualsIgnoreHosts(const DnsConfig& d) const;

  bool IsValid() const {
    return !nameservers.empty();
  }

  // List of name server addresses.
  std::vector<IPEndPoint> nameservers;
  // Suffix search list; used on first lookup when number of dots in given name
  // is less than |ndots|.
  std::vector<std::string> search;

  DnsHosts hosts;

  // Resolver options; see man resolv.conf.
  // TODO(szym): use |ndots| and |search| to determine the sequence of FQDNs
  // to query given a specific name.
  // TODO(szym): implement DNS Devolution for windows

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
class NET_EXPORT_PRIVATE DnsConfigService
  : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Callback interface for the client. The observer is called on the same
  // thread as Watch(). Observer must outlive the service.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called only when |dns_config| is different from the last call.
    virtual void OnConfigChanged(const DnsConfig& dns_config) = 0;
  };

  // Creates the platform-specific DnsConfigService.
  static DnsConfigService* CreateSystemService();

  DnsConfigService();
  virtual ~DnsConfigService();

  // Immediately starts watching system configuration for changes and attempts
  // to read the configuration. For some platform implementations, the current
  // thread must have an IO loop (for base::files::FilePathWatcher).
  virtual void Watch() {}

  // If a config is available, |observer| will immediately be called with
  // OnConfigChanged.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  FRIEND_TEST_ALL_PREFIXES(DnsConfigServiceTest, NotifyOnChange);
  friend class DnsHostsReader;
  // To be called with new config. |config|.hosts is ignored.
  virtual void OnConfigRead(const DnsConfig& config);
  // To be called with new hosts. Rest of the config is assumed unchanged.
  virtual void OnHostsRead(const DnsHosts& hosts);

  DnsConfig dns_config_;
  // True after first OnConfigRead and OnHostsRead; indicate complete config.
  bool have_config_;
  bool have_hosts_;

  ObserverList<Observer> observers_;
 private:
  DISALLOW_COPY_AND_ASSIGN(DnsConfigService);
};

// A WatchingFileReader that reads a HOSTS file and notifies
// DnsConfigService::OnHostsRead().
class DnsHostsReader : public WatchingFileReader {
 public:
  explicit DnsHostsReader(DnsConfigService* service);

  virtual void DoRead() OVERRIDE;
  virtual void OnReadFinished() OVERRIDE;

 private:
  virtual ~DnsHostsReader();

  DnsConfigService* service_;
  // Written in DoRead, read in OnReadFinished, no locking necessary.
  DnsHosts dns_hosts_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(DnsHostsReader);
};

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_H_
