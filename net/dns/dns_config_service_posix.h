// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#define NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
#pragma once

#include <resolv.h>

#include "base/compiler_specific.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"
#include "net/dns/dns_config_service.h"

namespace net {

class NET_EXPORT_PRIVATE DnsConfigServicePosix
  : public NON_EXPORTED_BASE(DnsConfigService),
    public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  class ResolverLib;
  class FilePathWatcherShim;
  class FilePathWatcherFactory;
  class WatcherDelegate;

  DnsConfigServicePosix();
  virtual ~DnsConfigServicePosix();

  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void Watch() OVERRIDE;

  // Takes ownership of |lib|. Must be set before Watch.
  void set_resolver_lib(ResolverLib* lib) {
    DCHECK(!watcher_delegate_.get());
    resolver_lib_.reset(lib);
  }

  // Takes ownership of |factory|. Must be set before Watch.
  void set_watcher_factory(FilePathWatcherFactory* factory) {
    DCHECK(!watcher_delegate_.get());
    watcher_factory_.reset(factory);
  }

 private:
  void OnConfigRead(const DnsConfig& config);

  // Configure a FilePathWatcher and install watcher_delegate_. Executed each
  // time FilePathWatcher fails.
  void StartWatch();

  DnsConfig dns_config_;
  // True after first OnConfigChanged, that is, dns_config_ is valid.
  bool have_config_;

  scoped_ptr<ResolverLib> resolver_lib_;
  scoped_ptr<FilePathWatcherFactory> watcher_factory_;
  scoped_ptr<FilePathWatcherShim> resolv_file_watcher_;
  scoped_refptr<WatcherDelegate> watcher_delegate_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(DnsConfigServicePosix);
};


// Allows mocking res_ninit.
class NET_EXPORT_PRIVATE DnsConfigServicePosix::ResolverLib
  : public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  ResolverLib() {}
  virtual ~ResolverLib() {}
  virtual int ninit(res_state statp);
  virtual void nclose(res_state statp);
 private:
  DISALLOW_COPY_AND_ASSIGN(ResolverLib);
};

// Allows mocking FilePathWatcher
class NET_EXPORT_PRIVATE DnsConfigServicePosix::FilePathWatcherShim
  : public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  FilePathWatcherShim();
  virtual ~FilePathWatcherShim();

  virtual bool Watch(
      const FilePath& path,
      base::files::FilePathWatcher::Delegate* delegate) WARN_UNUSED_RESULT;
 private:
  scoped_ptr<base::files::FilePathWatcher> watcher_;
  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherShim);
};

class NET_EXPORT_PRIVATE DnsConfigServicePosix::FilePathWatcherFactory
  : public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  FilePathWatcherFactory() {}
  virtual ~FilePathWatcherFactory() {}
  virtual FilePathWatcherShim* CreateFilePathWatcher();
 private:
  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherFactory);
};

// Fills in |dns_config| from |res|. Exposed for tests.
bool NET_EXPORT_PRIVATE ConvertResToConfig(const struct __res_state& res,
                                           DnsConfig* dns_config);

}  // namespace net

#endif  // NET_DNS_DNS_CONFIG_SERVICE_POSIX_H_
