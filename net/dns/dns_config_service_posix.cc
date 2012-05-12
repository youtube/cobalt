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
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/dns/file_path_watcher_wrapper.h"
#include "net/dns/serial_worker.h"

#if defined(OS_MACOSX)
#include "net/dns/notify_watcher_mac.h"
#endif

#ifndef _PATH_RESCONF  // Normally defined in <resolv.h>
#define _PATH_RESCONF "/etc/resolv.conf"
#endif

namespace net {

namespace {

const FilePath::CharType* kFilePathHosts = FILE_PATH_LITERAL("/etc/hosts");

// A SerialWorker that uses libresolv to initialize res_state and converts
// it to DnsConfig.
class ConfigReader : public SerialWorker {
 public:
  typedef base::Callback<void(const DnsConfig& config)> CallbackType;
  explicit ConfigReader(const CallbackType& callback)
      : callback_(callback),
        success_(false) {}

  void DoWork() OVERRIDE {
    success_ = false;
#if defined(OS_ANDROID)
    NOTIMPLEMENTED();
#elif defined(OS_OPENBSD)
    // Note: res_ninit in glibc always returns 0 and sets RES_INIT.
    // res_init behaves the same way.
    memset(&_res, 0, sizeof(_res));
    if ((res_init() == 0) && (_res.options & RES_INIT)) {
      success_ = internal::ConvertResStateToDnsConfig(_res, &dns_config_);
    }
#else  // all other OS_POSIX
    struct __res_state res;
    memset(&res, 0, sizeof(res));
    if ((res_ninit(&res) == 0) && (res.options & RES_INIT)) {
      success_ = internal::ConvertResStateToDnsConfig(res, &dns_config_);
    }
    // Prefer res_ndestroy where available.
#if defined(OS_MACOSX) || defined(OS_FREEBSD)
    res_ndestroy(&res);
#else
    res_nclose(&res);
#endif

#endif
  }

  void OnWorkFinished() OVERRIDE {
    DCHECK(!IsCancelled());
    if (success_)
      callback_.Run(dns_config_);
  }

 private:
  virtual ~ConfigReader() {}

  CallbackType callback_;
  // Written in DoWork, read in OnWorkFinished, no locking necessary.
  DnsConfig dns_config_;
  bool success_;
};

}  // namespace

namespace internal {

#if defined(OS_MACOSX)
// From 10.7.3 configd-395.10/dnsinfo/dnsinfo.h
static const char* kDnsNotifyKey =
    "com.apple.system.SystemConfiguration.dns_configuration";

class DnsConfigServicePosix::ConfigWatcher : public NotifyWatcherMac {
 public:
  bool Watch(const base::Callback<void(bool succeeded)>& callback) {
    return NotifyWatcherMac::Watch(kDnsNotifyKey, callback);
  }
};
#else
static const FilePath::CharType* kFilePathConfig =
    FILE_PATH_LITERAL(_PATH_RESCONF);

class DnsConfigServicePosix::ConfigWatcher : public FilePathWatcherWrapper {
 public:
  bool Watch(const base::Callback<void(bool succeeded)>& callback) {
    return FilePathWatcherWrapper::Watch(FilePath(kFilePathConfig), callback);
  }
};
#endif

DnsConfigServicePosix::DnsConfigServicePosix()
    : config_watcher_(new ConfigWatcher()),
      hosts_watcher_(new FilePathWatcherWrapper()) {
  config_reader_ = new ConfigReader(
      base::Bind(&DnsConfigServicePosix::OnConfigRead,
                 base::Unretained(this)));
  hosts_reader_ = new DnsHostsReader(
      FilePath(kFilePathHosts),
      base::Bind(&DnsConfigServicePosix::OnHostsRead,
                 base::Unretained(this)));
}

DnsConfigServicePosix::~DnsConfigServicePosix() {
  config_reader_->Cancel();
  hosts_reader_->Cancel();
}

void DnsConfigServicePosix::Watch(const CallbackType& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  set_callback(callback);

  // Even if watchers fail, we keep the other one as it provides useful signals.
  if (config_watcher_->Watch(
          base::Bind(&DnsConfigServicePosix::OnConfigChanged,
                     base::Unretained(this)))) {
    OnConfigChanged(true);
  } else {
    OnConfigChanged(false);
  }

  if (hosts_watcher_->Watch(
          FilePath(kFilePathHosts),
          base::Bind(&DnsConfigServicePosix::OnHostsChanged,
                     base::Unretained(this)))) {
    OnHostsChanged(true);
  } else {
    OnHostsChanged(false);
  }
}

void DnsConfigServicePosix::OnConfigChanged(bool watch_succeeded) {
  InvalidateConfig();
  // We don't trust a config that we cannot watch in the future.
  // TODO(szym): re-start watcher if that makes sense. http://crbug.com/116139
  if (watch_succeeded)
    config_reader_->WorkNow();
  else
    LOG(ERROR) << "Failed to watch DNS config";
}

void DnsConfigServicePosix::OnHostsChanged(bool watch_succeeded) {
  InvalidateHosts();
  if (watch_succeeded)
    hosts_reader_->WorkNow();
  else
    LOG(ERROR) << "Failed to watch DNS hosts";
}

#if !defined(OS_ANDROID)
bool ConvertResStateToDnsConfig(const struct __res_state& res,
                                DnsConfig* dns_config) {
  CHECK(dns_config != NULL);
  DCHECK(res.options & RES_INIT);

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
      return false;
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
      return false;
    }
    if (!ipe.FromSockAddr(addr, addr_len))
      return false;
    dns_config->nameservers.push_back(ipe);
  }
#else  // !(defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_FREEBSD))
  DCHECK_LE(res.nscount, MAXNS);
  for (int i = 0; i < res.nscount; ++i) {
    IPEndPoint ipe;
    if (!ipe.FromSockAddr(
        reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]),
        sizeof res.nsaddr_list[i])) {
      return false;
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

  return true;
}
#endif  // !defined(OS_ANDROID)

}  // namespace internal

// static
scoped_ptr<DnsConfigService> DnsConfigService::CreateSystemService() {
  return scoped_ptr<DnsConfigService>(new internal::DnsConfigServicePosix());
}

}  // namespace net
