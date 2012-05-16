// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_WIN_H_
#define NET_DNS_DNS_CONFIG_SERVICE_WIN_H_
#pragma once

// The sole purpose of dns_config_service_win.h is for unittests so we just
// include these headers here.
#include <winsock2.h>
#include <iphlpapi.h>

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

// The general effort of DnsConfigServiceWin is to configure |nameservers| and
// |search| in DnsConfig. The settings are stored in the Windows registry, but
// to simplify the task we use the IP Helper API wherever possible. That API
// yields the complete and ordered |nameservers|, but to determine |search| we
// need to use the registry. On Windows 7, WMI does return the correct |search|
// but on earlier versions it is insufficient.
//
// Experimental evaluation of Windows behavior suggests that domain parsing is
// naive. Domain suffixes in |search| are not validated until they are appended
// to the resolved name. We attempt to replicate this behavior.

namespace net {

namespace internal {

// Registry key paths.
const wchar_t* const kTcpipPath =
    L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters";
const wchar_t* const kTcpip6Path =
    L"SYSTEM\\CurrentControlSet\\Services\\Tcpip6\\Parameters";
const wchar_t* const kDnscachePath =
    L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters";
const wchar_t* const kPolicyPath =
    L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient";

// Returns the path to the HOSTS file.
FilePath GetHostsPath();

// Parses |value| as search list (comma-delimited list of domain names) from
// a registry key and stores it in |out|. Returns true on success. Empty
// entries (e.g., "chromium.org,,org") terminate the list. Non-ascii hostnames
// are converted to punycode.
bool NET_EXPORT_PRIVATE ParseSearchList(const string16& value,
                                        std::vector<std::string>* out);

// All relevant settings read from registry and IP Helper. This isolates our
// logic from system calls and is exposed for unit tests. Keep it an aggregate
// struct for easy initialization.
struct NET_EXPORT_PRIVATE DnsSystemSettings {
  // The |set| flag distinguishes between empty and unset values.
  struct RegString {
    bool set;
    string16 value;
  };

  struct RegDword {
    bool set;
    DWORD value;
  };

  struct DevolutionSetting {
    // UseDomainNameDevolution
    RegDword enabled;
    // DomainNameDevolutionLevel
    RegDword level;
  };

  // Filled in by GetAdapterAddresses. Note that the alternative
  // GetNetworkParams does not include IPv6 addresses.
  scoped_ptr_malloc<IP_ADAPTER_ADDRESSES> addresses;

  // SOFTWARE\Policies\Microsoft\Windows NT\DNSClient\SearchList
  RegString policy_search_list;
  // SYSTEM\CurrentControlSet\Tcpip\Parameters\SearchList
  RegString tcpip_search_list;
  // SYSTEM\CurrentControlSet\Tcpip\Parameters\Domain
  RegString tcpip_domain;
  // SOFTWARE\Policies\Microsoft\System\DNSClient\PrimaryDnsSuffix
  RegString primary_dns_suffix;

  // SOFTWARE\Policies\Microsoft\Windows NT\DNSClient
  DevolutionSetting policy_devolution;
  // SYSTEM\CurrentControlSet\Dnscache\Parameters
  DevolutionSetting dnscache_devolution;
  // SYSTEM\CurrentControlSet\Tcpip\Parameters
  DevolutionSetting tcpip_devolution;

  // SOFTWARE\Policies\Microsoft\Windows NT\DNSClient\AppendToMultiLabelName
  RegDword append_to_multi_label_name;
};

// Fills in |dns_config| from |settings|. Exposed for tests.
bool NET_EXPORT_PRIVATE ConvertSettingsToDnsConfig(
    const DnsSystemSettings& settings,
    DnsConfig* dns_config);

// Use DnsConfigService::CreateSystemService to use it outside of tests.
class NET_EXPORT_PRIVATE DnsConfigServiceWin
    : public DnsConfigService,
      public NetworkChangeNotifier::IPAddressObserver {
 public:
  DnsConfigServiceWin();
  virtual ~DnsConfigServiceWin();

  virtual void Watch(const CallbackType& callback) OVERRIDE;

 private:
  class ConfigReader;
  class HostsReader;

  // NetworkChangeNotifier::DNSObserver:
  virtual void OnDNSChanged(unsigned detail) OVERRIDE;

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  scoped_refptr<ConfigReader> config_reader_;
  scoped_refptr<HostsReader> hosts_reader_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServiceWin);
};

}  // namespace internal

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_WIN_H_

