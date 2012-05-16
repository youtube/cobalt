// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_H_
#define NET_DNS_DNS_CONFIG_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "base/timer.h"
// Needed on shared build with MSVS2010 to avoid multiple definitions of
// std::vector<IPEndPoint>.
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"  // win requires size of IPEndPoint
#include "net/base/network_change_notifier.h"
#include "net/base/net_export.h"
#include "net/dns/dns_hosts.h"

namespace base {
class Value;
}

namespace net {

// DnsConfig stores configuration of the system resolver.
struct NET_EXPORT_PRIVATE DnsConfig {
  DnsConfig();
  virtual ~DnsConfig();

  bool Equals(const DnsConfig& d) const;

  bool EqualsIgnoreHosts(const DnsConfig& d) const;

  void CopyIgnoreHosts(const DnsConfig& src);

  // Returns a Value representation of |this|.  Caller takes ownership of the
  // returned Value.  For performance reasons, the Value only contains the
  // number of hosts rather than the full list.
  base::Value* ToValue() const;

  bool IsValid() const {
    return !nameservers.empty();
  }

  // List of name server addresses.
  std::vector<IPEndPoint> nameservers;
  // Suffix search list; used on first lookup when number of dots in given name
  // is less than |ndots|.
  std::vector<std::string> search;

  DnsHosts hosts;

  // AppendToMultiLabelName: is suffix search performed for multi-label names?
  // True, except on Windows where it can be configured.
  bool append_to_multi_label_name;

  // Resolver options; see man resolv.conf.

  // Minimum number of dots before global resolution precedes |search|.
  int ndots;
  // Time between retransmissions, see res_state.retrans.
  base::TimeDelta timeout;
  // Maximum number of attempts, see res_state.retry.
  int attempts;
  // Round robin entries in |nameservers| for subsequent requests.
  bool rotate;
  // Enable EDNS0 extensions.
  bool edns0;
};


// Service for reading system DNS settings, on demand or when signalled by
// NetworkChangeNotifier.
class NET_EXPORT_PRIVATE DnsConfigService
    : NON_EXPORTED_BASE(public base::NonThreadSafe),
      public NetworkChangeNotifier::DNSObserver {
 public:
  // Callback interface for the client, called on the same thread as Read() and
  // Watch().
  typedef base::Callback<void(const DnsConfig& config)> CallbackType;

  // Creates the platform-specific DnsConfigService.
  static scoped_ptr<DnsConfigService> CreateSystemService();

  DnsConfigService();
  virtual ~DnsConfigService();

  // Attempts to read the configuration. Will run |callback| when succeeded.
  // Can be called at most once.
  void Read(const CallbackType& callback);

  // Registers for notifications at NetworkChangeNotifier. Will attempt to read
  // config after watch is started by NetworkChangeNotifier. Will run |callback|
  // iff config changes from last call or should be withdrawn.
  // Can be called at most once.
  virtual void Watch(const CallbackType& callback);

 protected:
  friend class DnsHostsReader;

  // Called when the current config (except hosts) has changed.
  void InvalidateConfig();
  // Called when the current hosts have changed.
  void InvalidateHosts();

  // Called with new config. |config|.hosts is ignored.
  void OnConfigRead(const DnsConfig& config);
  // Called with new hosts. Rest of the config is assumed unchanged.
  void OnHostsRead(const DnsHosts& hosts);

  // NetworkChangeNotifier::DNSObserver:
  // Must be defined by implementations.
  virtual void OnDNSChanged(unsigned detail) OVERRIDE = 0;

 private:
  void StartTimer();
  // Called when the timer expires.
  void OnTimeout();
  // Called when the config becomes complete.
  void OnCompleteConfig();

  CallbackType callback_;

  DnsConfig dns_config_;

  // True after On*Read, before Invalidate*. Tells if the config is complete.
  bool have_config_;
  bool have_hosts_;
  // True if receiver needs to be updated when the config becomes complete.
  bool need_update_;

  // Started in Invalidate*, cleared in On*Read.
  base::OneShotTimer<DnsConfigService> timer_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigService);
};

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_H_
