// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_posix.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/serial_worker.h"

namespace net {

#if !defined(OS_ANDROID)
namespace internal {

namespace {

#ifndef _PATH_RESCONF  // Normally defined in <resolv.h>
#define _PATH_RESCONF "/etc/resolv.conf"
#endif

const FilePath::CharType* kFilePathHosts = FILE_PATH_LITERAL("/etc/hosts");

ConfigParsePosixResult ReadDnsConfig(DnsConfig* config) {
  ConfigParsePosixResult result;
#if defined(OS_OPENBSD)
  // Note: res_ninit in glibc always returns 0 and sets RES_INIT.
  // res_init behaves the same way.
  memset(&_res, 0, sizeof(_res));
  if (res_init() == 0) {
    result = ConvertResStateToDnsConfig(_res, config);
  } else {
    result = CONFIG_PARSE_POSIX_RES_INIT_FAILED;
  }
#else  // all other OS_POSIX
  struct __res_state res;
  memset(&res, 0, sizeof(res));
  if (res_ninit(&res) == 0) {
    result = ConvertResStateToDnsConfig(res, config);
  } else {
    result = CONFIG_PARSE_POSIX_RES_INIT_FAILED;
  }
  // Prefer res_ndestroy where available.
#if defined(OS_MACOSX) || defined(OS_FREEBSD)
  res_ndestroy(&res);
#else
  res_nclose(&res);
#endif
#endif
  return result;
}

// A SerialWorker that uses libresolv to initialize res_state and converts
// it to DnsConfig.
class ConfigReader : public SerialWorker {
 public:
  typedef base::Callback<void(const DnsConfig& config)> CallbackType;
  explicit ConfigReader(const CallbackType& callback)
      : callback_(callback), success_(false) {}

  void DoWork() OVERRIDE {
    base::TimeTicks start_time = base::TimeTicks::Now();
    ConfigParsePosixResult result = ReadDnsConfig(&dns_config_);
    success_ = (result == CONFIG_PARSE_POSIX_OK);
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.ConfigParsePosix",
                              result, CONFIG_PARSE_POSIX_MAX);
    UMA_HISTOGRAM_BOOLEAN("AsyncDNS.ConfigParseResult", success_);
    UMA_HISTOGRAM_TIMES("AsyncDNS.ConfigParseDuration",
                        base::TimeTicks::Now() - start_time);
  }

  void OnWorkFinished() OVERRIDE {
    DCHECK(!IsCancelled());
    if (success_) {
      callback_.Run(dns_config_);
    } else {
      LOG(WARNING) << "Failed to read DnsConfig.";
    }
  }

 private:
  virtual ~ConfigReader() {}

  const CallbackType callback_;
  // Written in DoWork, read in OnWorkFinished, no locking necessary.
  DnsConfig dns_config_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(ConfigReader);
};

// A SerialWorker that reads the HOSTS file and runs Callback.
class HostsReader : public SerialWorker {
 public:
  typedef base::Callback<void(const DnsHosts& hosts)> CallbackType;
  explicit HostsReader(const CallbackType& callback)
      : path_(kFilePathHosts), callback_(callback), success_(false) {}

 private:
  virtual ~HostsReader() {}

  virtual void DoWork() OVERRIDE {
    base::TimeTicks start_time = base::TimeTicks::Now();
    success_ = ParseHostsFile(path_, &hosts_);
    UMA_HISTOGRAM_BOOLEAN("AsyncDNS.HostParseResult", success_);
    UMA_HISTOGRAM_TIMES("AsyncDNS.HostsParseDuration",
                        base::TimeTicks::Now() - start_time);
  }

  virtual void OnWorkFinished() OVERRIDE {
    if (success_) {
      callback_.Run(hosts_);
    } else {
      LOG(WARNING) << "Failed to read DnsHosts.";
    }
  }

  const FilePath path_;
  const CallbackType callback_;
  // Written in DoWork, read in OnWorkFinished, no locking necessary.
  DnsHosts hosts_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(HostsReader);
};

}  // namespace

DnsConfigServicePosix::DnsConfigServicePosix() {
  config_reader_ = new ConfigReader(
      base::Bind(&DnsConfigServicePosix::OnConfigRead,
                 base::Unretained(this)));
  hosts_reader_ = new HostsReader(
      base::Bind(&DnsConfigServicePosix::OnHostsRead,
                 base::Unretained(this)));
}

DnsConfigServicePosix::~DnsConfigServicePosix() {
  config_reader_->Cancel();
  hosts_reader_->Cancel();
}

void DnsConfigServicePosix::OnDNSChanged(unsigned detail) {
  if (detail & NetworkChangeNotifier::CHANGE_DNS_WATCH_FAILED) {
    InvalidateConfig();
    InvalidateHosts();
    // We don't trust a config that we cannot watch in the future.
    config_reader_->Cancel();
    hosts_reader_->Cancel();
    return;
  }
  if (detail & NetworkChangeNotifier::CHANGE_DNS_WATCH_STARTED)
    detail = ~0;  // Assume everything changed.
  if (detail & NetworkChangeNotifier::CHANGE_DNS_SETTINGS) {
    InvalidateConfig();
    config_reader_->WorkNow();
  }
  if (detail & NetworkChangeNotifier::CHANGE_DNS_HOSTS) {
    InvalidateHosts();
    hosts_reader_->WorkNow();
  }
}

ConfigParsePosixResult ConvertResStateToDnsConfig(const struct __res_state& res,
                                                  DnsConfig* dns_config) {
  CHECK(dns_config != NULL);
  if (!(res.options & RES_INIT))
    return CONFIG_PARSE_POSIX_RES_INIT_UNSET;

  dns_config->nameservers.clear();

#if defined(OS_MACOSX) || defined(OS_FREEBSD)
  union res_sockaddr_union addresses[MAXNS];
  int nscount = res_getservers(const_cast<res_state>(&res), addresses, MAXNS);
  DCHECK_GE(nscount, 0);
  DCHECK_LE(nscount, MAXNS);
  for (int i = 0; i < nscount; ++i) {
    IPEndPoint ipe;
    if (!ipe.FromSockAddr(
            reinterpret_cast<const struct sockaddr*>(&addresses[i]),
            sizeof addresses[i])) {
      return CONFIG_PARSE_POSIX_BAD_ADDRESS;
    }
    dns_config->nameservers.push_back(ipe);
  }
#elif defined(OS_LINUX)
  COMPILE_ASSERT(arraysize(res.nsaddr_list) >= MAXNS &&
                 arraysize(res._u._ext.nsaddrs) >= MAXNS,
                 incompatible_libresolv_res_state);
  DCHECK_LE(res.nscount, MAXNS);
  // Initially, glibc stores IPv6 in |_ext.nsaddrs| and IPv4 in |nsaddr_list|.
  // In res_send.c:res_nsend, it merges |nsaddr_list| into |nsaddrs|,
  // but we have to combine the two arrays ourselves.
  for (int i = 0; i < res.nscount; ++i) {
    IPEndPoint ipe;
    const struct sockaddr* addr = NULL;
    size_t addr_len = 0;
    if (res.nsaddr_list[i].sin_family) {  // The indicator used by res_nsend.
      addr = reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]);
      addr_len = sizeof res.nsaddr_list[i];
    } else if (res._u._ext.nsaddrs[i] != NULL) {
      addr = reinterpret_cast<const struct sockaddr*>(res._u._ext.nsaddrs[i]);
      addr_len = sizeof *res._u._ext.nsaddrs[i];
    } else {
      return CONFIG_PARSE_POSIX_BAD_EXT_STRUCT;
    }
    if (!ipe.FromSockAddr(addr, addr_len))
      return CONFIG_PARSE_POSIX_BAD_ADDRESS;
    dns_config->nameservers.push_back(ipe);
  }
#else  // !(defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_FREEBSD))
  DCHECK_LE(res.nscount, MAXNS);
  for (int i = 0; i < res.nscount; ++i) {
    IPEndPoint ipe;
    if (!ipe.FromSockAddr(
            reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]),
            sizeof res.nsaddr_list[i])) {
      return CONFIG_PARSE_POSIX_BAD_ADDRESS;
    }
    dns_config->nameservers.push_back(ipe);
  }
#endif

  dns_config->search.clear();
  for (int i = 0; (i < MAXDNSRCH) && res.dnsrch[i]; ++i) {
    dns_config->search.push_back(std::string(res.dnsrch[i]));
  }

  dns_config->ndots = res.ndots;
  dns_config->timeout = base::TimeDelta::FromSeconds(res.retrans);
  dns_config->attempts = res.retry;
#if defined(RES_ROTATE)
  dns_config->rotate = res.options & RES_ROTATE;
#endif
  dns_config->edns0 = res.options & RES_USE_EDNS0;

  // The current implementation assumes these options are set. They normally
  // cannot be overwritten by /etc/resolv.conf
  unsigned kRequiredOptions = RES_RECURSE | RES_DEFNAMES | RES_DNSRCH;
  if ((res.options & kRequiredOptions) != kRequiredOptions)
    return CONFIG_PARSE_POSIX_MISSING_OPTIONS;

  unsigned kUnhandledOptions = RES_USEVC | RES_IGNTC | RES_USE_DNSSEC;
  if (res.options & kUnhandledOptions)
    return CONFIG_PARSE_POSIX_UNHANDLED_OPTIONS;

  if (dns_config->nameservers.empty())
    return CONFIG_PARSE_POSIX_NO_NAMESERVERS;

  // If any name server is 0.0.0.0, assume the configuration is invalid.
  // TODO(szym): Measure how often this happens. http://crbug.com/125599
  const IPAddressNumber kEmptyAddress(kIPv4AddressSize);
  for (unsigned i = 0; i < dns_config->nameservers.size(); ++i) {
    if (dns_config->nameservers[i].address() == kEmptyAddress)
      return CONFIG_PARSE_POSIX_NULL_ADDRESS;
  }
  return CONFIG_PARSE_POSIX_OK;
}

}  // namespace internal

// static
scoped_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return scoped_ptr<DnsConfigService>(new internal::DnsConfigServicePosix());
}

#else  // defined(OS_ANDROID)
// Android NDK provides only a stub <resolv.h> header.
class StubDnsConfigService : public DnsConfigService {
 public:
  StubDnsConfigService() {}
  virtual ~StubDnsConfigService() {}
  virtual void OnDNSChanged(unsigned detail) OVERRIDE {}
};
// static
scoped_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return scoped_ptr<DnsConfigService>(new StubDnsConfigService());
}
#endif

}  // namespace net
