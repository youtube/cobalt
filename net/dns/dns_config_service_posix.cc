// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_posix.h"

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/threading/worker_pool.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"

#ifndef _PATH_RESCONF  // Normally defined in <resolv.h>
#define _PATH_RESCONF "/etc/resolv.conf"
#endif

namespace net {

// FilePathWatcher::Delegate is refcounted, so we separate it from the Service
// It also hosts callbacks on the WorkerPool.
class DnsConfigServicePosix::WatcherDelegate
  : public base::files::FilePathWatcher::Delegate {
 public:
  // Takes ownership of |lib|.
  WatcherDelegate(DnsConfigServicePosix* service,
                  DnsConfigServicePosix::ResolverLib* lib)
    : service_(service),
      resolver_lib_(lib),
      message_loop_(base::MessageLoopProxy::current()),
      reading_(false),
      read_pending_(false) {}

  void Cancel() {
    DCHECK(message_loop_->BelongsToCurrentThread());
    service_ = NULL;
  }

  void RescheduleWatch() {
    DCHECK(message_loop_->BelongsToCurrentThread());
    // Retry Watch in 100ms or so.
    message_loop_->PostDelayedTask(
        FROM_HERE, base::Bind(&WatcherDelegate::StartWatch, this), 100);
  }

  // FilePathWatcher::Delegate interface
  virtual void OnFilePathChanged(const FilePath& path) OVERRIDE {
    DCHECK(message_loop_->BelongsToCurrentThread());
    if (!service_)
      return;
    ScheduleRead();
  }

  virtual void OnFilePathError(const FilePath& path) OVERRIDE {
    DCHECK(message_loop_->BelongsToCurrentThread());
    StartWatch();
  }

 private:
  virtual ~WatcherDelegate() {}

  // Unless already scheduled, post DoRead to WorkerPool.
  void ScheduleRead() {
    if (reading_) {
      // Mark that we need to re-read after DoRead posts results.
      read_pending_ = true;
    } else {
      if (!base::WorkerPool::PostTask(FROM_HERE, base::Bind(
          &WatcherDelegate::DoRead, this), false)) {
        // See worker_pool_posix.cc.
        NOTREACHED() << "WorkerPool::PostTask is not expected to fail on posix";
      }
      reading_ = true;
      read_pending_ = false;
    }
  }

  // Reads DnsConfig and posts OnResultAvailable to |message_loop_|.
  // Must be called on the worker thread.
  void DoRead() {
    DnsConfig config;
    struct __res_state res;
    bool success = false;
    if (resolver_lib_->ninit(&res) == 0) {
      success = ConvertResToConfig(res, &config);
      resolver_lib_->nclose(&res);
    }
    // If this fails, the loop is gone, so there is no point retrying.
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &WatcherDelegate::OnResultAvailable, this, config, success));
  }

  // Communicates result to the service. Must be called on the the same thread
  // that constructed WatcherDelegate.
  void OnResultAvailable(const DnsConfig &config, bool success) {
    DCHECK(message_loop_->BelongsToCurrentThread());
    if (!service_)
      return;
    reading_ = false;
    if (read_pending_) {
      // Discard this result and re-schedule.
      ScheduleRead();
      return;
    }
    if (!success) {
      VLOG(1) << "Failed to read DnsConfig";
    } else {
      service_->OnConfigRead(config);
    }
  }

  // To avoid refcounting the service, use this method in tasks/callbacks.
  void StartWatch() {
    if (!service_)
      return;
    service_->StartWatch();
  }

  DnsConfigServicePosix* service_;
  scoped_ptr<DnsConfigServicePosix::ResolverLib> resolver_lib_;
  // Message loop for the thread on which Watch is called (of TYPE_IO).
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  // True after DoRead before OnResultsAvailable.
  bool reading_;
  // True after OnFilePathChanged fires while |reading_| is true.
  bool read_pending_;
};

DnsConfigServicePosix::DnsConfigServicePosix()
  : have_config_(false),
    resolver_lib_(new ResolverLib()),
    watcher_factory_(new FilePathWatcherFactory()) {
}

DnsConfigServicePosix::~DnsConfigServicePosix() {
  DCHECK(CalledOnValidThread());
  if (watcher_delegate_.get())
    watcher_delegate_->Cancel();
}

void DnsConfigServicePosix::AddObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
  if (have_config_) {
    observer->OnConfigChanged(dns_config_);
  }
}

void DnsConfigServicePosix::RemoveObserver(Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void DnsConfigServicePosix::Watch() {
  DCHECK(CalledOnValidThread());
  DCHECK(!watcher_delegate_.get());
  DCHECK(!resolv_file_watcher_.get());
  DCHECK(resolver_lib_.get());
  DCHECK(watcher_factory_.get());

  watcher_delegate_ = new WatcherDelegate(this, resolver_lib_.release());
  StartWatch();
}

void DnsConfigServicePosix::OnConfigRead(const DnsConfig& config) {
  DCHECK(CalledOnValidThread());
  if (!config.Equals(dns_config_)) {
    dns_config_ = config;
    have_config_ = true;
    FOR_EACH_OBSERVER(Observer, observers_, OnConfigChanged(config));
  }
}

void DnsConfigServicePosix::StartWatch() {
  DCHECK(CalledOnValidThread());
  DCHECK(watcher_delegate_.get());

  FilePath path(FILE_PATH_LITERAL(_PATH_RESCONF));

  // FilePathWatcher allows only one Watch per lifetime, so we need a new one.
  resolv_file_watcher_.reset(watcher_factory_->CreateFilePathWatcher());
  if (resolv_file_watcher_->Watch(path, watcher_delegate_.get())) {
    // Make the initial read after watch is installed.
    watcher_delegate_->OnFilePathChanged(path);
  } else {
    VLOG(1) << "Watch failed, scheduling restart";
    watcher_delegate_->RescheduleWatch();
  }
}

int DnsConfigServicePosix::ResolverLib::ninit(res_state statp) {
  return ::res_ninit(statp);
}

void DnsConfigServicePosix::ResolverLib::nclose(res_state statp) {
  return ::res_nclose(statp);
}

DnsConfigServicePosix::FilePathWatcherShim::FilePathWatcherShim()
  : watcher_(new base::files::FilePathWatcher()) {}
DnsConfigServicePosix::FilePathWatcherShim::~FilePathWatcherShim() {}

bool DnsConfigServicePosix::FilePathWatcherShim::Watch(
    const FilePath& path,
    base::files::FilePathWatcher::Delegate* delegate) {
  return watcher_->Watch(path, delegate);
}

DnsConfigServicePosix::FilePathWatcherShim*
DnsConfigServicePosix::FilePathWatcherFactory::CreateFilePathWatcher() {
  return new FilePathWatcherShim();
}

// static
DnsConfigService* DnsConfigService::CreateSystemService() {
  return new DnsConfigServicePosix();
}

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
  dns_config->rotate = res.options & RES_ROTATE;
  dns_config->edns0 = res.options & RES_USE_EDNS0;

  return true;
}

}  // namespace net
