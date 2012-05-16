// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
#pragma once

#include <SystemConfiguration/SystemConfiguration.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_config_watcher_mac.h"

namespace net {

class NetworkChangeNotifierMac: public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierMac();
  virtual ~NetworkChangeNotifierMac();

  // NetworkChangeNotifier:
  virtual bool IsCurrentlyOffline() const OVERRIDE;

 private:
  enum OnlineState {
    UNINITIALIZED = -1,
    OFFLINE = 0,
    ONLINE = 1
  };

  class DnsWatcherThread;

  // Forwarder just exists to keep the NetworkConfigWatcherMac API out of
  // NetworkChangeNotifierMac's public API.
  class Forwarder : public NetworkConfigWatcherMac::Delegate {
   public:
    explicit Forwarder(NetworkChangeNotifierMac* net_config_watcher)
        : net_config_watcher_(net_config_watcher) {}

    // NetworkConfigWatcherMac::Delegate implementation:
    virtual void Init() OVERRIDE {
      net_config_watcher_->SetInitialState();
    }
    virtual void StartReachabilityNotifications() OVERRIDE {
      net_config_watcher_->StartReachabilityNotifications();
    }
    virtual void SetDynamicStoreNotificationKeys(
        SCDynamicStoreRef store) OVERRIDE {
      net_config_watcher_->SetDynamicStoreNotificationKeys(store);
    }
    virtual void OnNetworkConfigChange(CFArrayRef changed_keys) OVERRIDE {
      net_config_watcher_->OnNetworkConfigChange(changed_keys);
    }

   private:
    NetworkChangeNotifierMac* const net_config_watcher_;
    DISALLOW_COPY_AND_ASSIGN(Forwarder);
  };

  // Methods directly called by the NetworkConfigWatcherMac::Delegate:
  void StartReachabilityNotifications();
  void SetDynamicStoreNotificationKeys(SCDynamicStoreRef store);
  void OnNetworkConfigChange(CFArrayRef changed_keys);

  void SetInitialState();

  static void ReachabilityCallback(SCNetworkReachabilityRef target,
                                   SCNetworkConnectionFlags flags,
                                   void* notifier);

  // These must be constructed before config_watcher_ to ensure
  // the lock is in a valid state when Forwarder::Init is called.
  OnlineState online_state_;
  mutable base::Lock online_state_lock_;
  mutable base::ConditionVariable initial_state_cv_;
  base::mac::ScopedCFTypeRef<SCNetworkReachabilityRef> reachability_;
  base::mac::ScopedCFTypeRef<CFRunLoopRef> run_loop_;

  Forwarder forwarder_;
  scoped_ptr<const NetworkConfigWatcherMac> config_watcher_;

  // Thread on which we can run DnsConfigWatcher, which requires TYPE_IO.
  scoped_ptr<DnsWatcherThread> dns_watcher_thread_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierMac);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_MAC_H_
