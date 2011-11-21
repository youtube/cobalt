// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_posix.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/dns/serial_worker.h"
#include "net/dns/watching_file_reader.h"

#ifndef _PATH_RESCONF  // Normally defined in <resolv.h>
#define _PATH_RESCONF "/etc/resolv.conf"
#endif

namespace net {

// A SerialWorker that uses ResolverLib to initialize res_state and converts
// it to DnsConfig.
class DnsConfigServicePosix::ConfigReader : public SerialWorker {
 public:
  explicit ConfigReader(DnsConfigServicePosix* service)
    : service_(service),
      success_(false) {}

  void DoWork() OVERRIDE {
    success_ = false;
#if defined(OS_ANDROID)
    NOTIMPLEMENTED();
#else
#if defined(OS_OPENBSD)
    // Note: res_ninit in glibc always returns 0 and sets RES_INIT.
    // res_init behaves the same way.
    if ((res_init() == 0) && (_res.options & RES_INIT)) {
      success_ = ConvertResToConfig(_res, &dns_config_);
    }
#else
    struct __res_state res;
    if ((res_ninit(&res) == 0) && (res.options & RES_INIT)) {
      success_ = ConvertResToConfig(res, &dns_config_);
    }
#endif
#if defined(OS_MACOSX)
    res_ndestroy(&res);
#elif !defined(OS_OPENBSD)
    res_nclose(&res);
#endif
#endif  // defined(OS_ANDROID)
  }

  void OnWorkFinished() OVERRIDE {
    DCHECK(!IsCancelled());
    if (success_)
      service_->OnConfigRead(dns_config_);
  }

 private:
  virtual ~ConfigReader() {}

  DnsConfigServicePosix* service_;
  // Written in DoWork, read in OnWorkFinished, no locking necessary.
  DnsConfig dns_config_;
  bool success_;
};

DnsConfigServicePosix::DnsConfigServicePosix()
  : config_watcher_(new WatchingFileReader()),
    hosts_watcher_(new WatchingFileReader()) {}

DnsConfigServicePosix::~DnsConfigServicePosix() {}

void DnsConfigServicePosix::Watch() {
  DCHECK(CalledOnValidThread());
  config_watcher_->StartWatch(FilePath(FILE_PATH_LITERAL(_PATH_RESCONF)),
                              new ConfigReader(this));
  FilePath hosts_path(FILE_PATH_LITERAL("/etc/hosts"));
  hosts_watcher_->StartWatch(hosts_path, new DnsHostsReader(hosts_path, this));
}

// static
DnsConfigService* DnsConfigService::CreateSystemService() {
  return new DnsConfigServicePosix();
}

#if !defined(OS_ANDROID)
bool ConvertResToConfig(const struct __res_state& res, DnsConfig* dns_config) {
  CHECK(dns_config != NULL);
  DCHECK(res.options & RES_INIT);

  dns_config->nameservers.clear();

#if defined(OS_LINUX)
  // Initially, glibc stores IPv6 in _ext.nsaddrs and IPv4 in nsaddr_list.
  // Next (res_send.c::__libc_res_nsend), it copies nsaddr_list after nsaddrs.
  // If RES_ROTATE is enabled, the list is shifted left after each res_send.
  // However, if nsaddr_list changes, it will refill nsaddr_list (IPv4) but
  // leave the IPv6 entries in nsaddr in the same (shifted) order.

  // Put IPv6 addresses ahead of IPv4.
  for (int i = 0; i < res._u._ext.nscount6; ++i) {
    IPEndPoint ipe;
    if (ipe.FromSockAddr(
        reinterpret_cast<const struct sockaddr*>(res._u._ext.nsaddrs[i]),
        sizeof *res._u._ext.nsaddrs[i])) {
      dns_config->nameservers.push_back(ipe);
    } else {
      return false;
    }
  }
#endif

  for (int i = 0; i < res.nscount; ++i) {
    IPEndPoint ipe;
    if (ipe.FromSockAddr(
        reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]),
        sizeof res.nsaddr_list[i])) {
      dns_config->nameservers.push_back(ipe);
    } else {
      return false;
    }
  }

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

  return true;
}
#endif  // !defined(OS_ANDROID)

}  // namespace net
