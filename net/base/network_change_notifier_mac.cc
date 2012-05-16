// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_mac.h"

#include <netinet/in.h>
#include <resolv.h>

#include "base/basictypes.h"
#include "base/threading/thread.h"
#include "net/dns/dns_config_watcher.h"

#ifndef _PATH_RESCONF  // Normally defined in <resolv.h>
#define _PATH_RESCONF "/etc/resolv.conf"
#endif

namespace net {

static bool CalculateReachability(SCNetworkConnectionFlags flags) {
  bool reachable = flags & kSCNetworkFlagsReachable;
  bool connection_required = flags & kSCNetworkFlagsConnectionRequired;
  return reachable && !connection_required;
}

class NetworkChangeNotifierMac::DnsWatcherThread : public base::Thread {
 public:
  DnsWatcherThread() : base::Thread("NetworkChangeNotifier") {}

  virtual ~DnsWatcherThread() {
    Stop();
  }

  virtual void Init() OVERRIDE {
    watcher_.Init();
  }

  virtual void CleanUp() OVERRIDE {
    watcher_.CleanUp();
  }

 private:
  internal::DnsConfigWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(DnsWatcherThread);
};

NetworkChangeNotifierMac::NetworkChangeNotifierMac()
    : online_state_(UNINITIALIZED),
      initial_state_cv_(&online_state_lock_),
      forwarder_(this),
      dns_watcher_thread_(new DnsWatcherThread()) {
  // Must be initialized after the rest of this object, as it may call back into
  // SetInitialState().
  config_watcher_.reset(new NetworkConfigWatcherMac(&forwarder_));
  dns_watcher_thread_->StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));
}

NetworkChangeNotifierMac::~NetworkChangeNotifierMac() {
  // Delete the ConfigWatcher to join the notifier thread, ensuring that
  // StartReachabilityNotifications() has an opportunity to run to completion.
  config_watcher_.reset();

  // Now that StartReachabilityNotifications() has either run to completion or
  // never run at all, unschedule reachability_ if it was previously scheduled.
  if (reachability_.get() && run_loop_.get()) {
    SCNetworkReachabilityUnscheduleFromRunLoop(reachability_.get(),
                                               run_loop_.get(),
                                               kCFRunLoopCommonModes);
  }
}

bool NetworkChangeNotifierMac::IsCurrentlyOffline() const {
  base::AutoLock lock(online_state_lock_);
  // Make sure the initial state is set before returning.
  while (online_state_ == UNINITIALIZED) {
    initial_state_cv_.Wait();
  }
  return online_state_ == OFFLINE;
}

void NetworkChangeNotifierMac::SetInitialState() {
  // Called on notifier thread.

  // Try to reach 0.0.0.0. This is the approach taken by Firefox:
  //
  // http://mxr.mozilla.org/mozilla2.0/source/netwerk/system/mac/nsNetworkLinkService.mm
  //
  // From my (adamk) testing on Snow Leopard, 0.0.0.0
  // seems to be reachable if any network connection is available.
  struct sockaddr_in addr = {0};
  addr.sin_len = sizeof(addr);
  addr.sin_family = AF_INET;
  reachability_.reset(SCNetworkReachabilityCreateWithAddress(
      kCFAllocatorDefault, reinterpret_cast<struct sockaddr*>(&addr)));

  SCNetworkConnectionFlags flags;
  bool reachable = true;
  if (SCNetworkReachabilityGetFlags(reachability_, &flags))
    reachable = CalculateReachability(flags);
  else
    LOG(ERROR) << "Could not get initial network state, assuming online.";
  {
    base::AutoLock lock(online_state_lock_);
    online_state_ = reachable ? ONLINE : OFFLINE;
    initial_state_cv_.Signal();
  }
}

void NetworkChangeNotifierMac::StartReachabilityNotifications() {
  // Called on notifier thread.
  run_loop_.reset(CFRunLoopGetCurrent());
  CFRetain(run_loop_.get());

  DCHECK(reachability_);
  SCNetworkReachabilityContext reachability_context = {
    0,     // version
    this,  // user data
    NULL,  // retain
    NULL,  // release
    NULL   // description
  };
  if (!SCNetworkReachabilitySetCallback(
          reachability_,
          &NetworkChangeNotifierMac::ReachabilityCallback,
          &reachability_context)) {
    LOG(DFATAL) << "Could not set network reachability callback";
    reachability_.reset();
  } else if (!SCNetworkReachabilityScheduleWithRunLoop(reachability_,
                                                       run_loop_,
                                                       kCFRunLoopCommonModes)) {
    LOG(DFATAL) << "Could not schedule network reachability on run loop";
    reachability_.reset();
  }
}

void NetworkChangeNotifierMac::SetDynamicStoreNotificationKeys(
    SCDynamicStoreRef store) {
  base::mac::ScopedCFTypeRef<CFMutableArrayRef> notification_keys(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  base::mac::ScopedCFTypeRef<CFStringRef> key(
      SCDynamicStoreKeyCreateNetworkGlobalEntity(
          NULL, kSCDynamicStoreDomainState, kSCEntNetInterface));
  CFArrayAppendValue(notification_keys.get(), key.get());
  key.reset(SCDynamicStoreKeyCreateNetworkGlobalEntity(
      NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4));
  CFArrayAppendValue(notification_keys.get(), key.get());
  key.reset(SCDynamicStoreKeyCreateNetworkGlobalEntity(
      NULL, kSCDynamicStoreDomainState, kSCEntNetIPv6));
  CFArrayAppendValue(notification_keys.get(), key.get());

  // Set the notification keys.  This starts us receiving notifications.
  bool ret = SCDynamicStoreSetNotificationKeys(
      store, notification_keys.get(), NULL);
  // TODO(willchan): Figure out a proper way to handle this rather than crash.
  CHECK(ret);
}

void NetworkChangeNotifierMac::OnNetworkConfigChange(CFArrayRef changed_keys) {
  DCHECK_EQ(run_loop_.get(), CFRunLoopGetCurrent());

  for (CFIndex i = 0; i < CFArrayGetCount(changed_keys); ++i) {
    CFStringRef key = static_cast<CFStringRef>(
        CFArrayGetValueAtIndex(changed_keys, i));
    if (CFStringHasSuffix(key, kSCEntNetIPv4) ||
        CFStringHasSuffix(key, kSCEntNetIPv6)) {
      NotifyObserversOfIPAddressChange();
      return;
    }
    if (CFStringHasSuffix(key, kSCEntNetInterface)) {
      // TODO(willchan): Does not appear to be working.  Look into this.
      // Perhaps this isn't needed anyway.
    } else {
      NOTREACHED();
    }
  }
}

// static
void NetworkChangeNotifierMac::ReachabilityCallback(
    SCNetworkReachabilityRef target,
    SCNetworkConnectionFlags flags,
    void* notifier) {
  NetworkChangeNotifierMac* notifier_mac =
      static_cast<NetworkChangeNotifierMac*>(notifier);

  DCHECK_EQ(notifier_mac->run_loop_.get(), CFRunLoopGetCurrent());

  OnlineState new_state = CalculateReachability(flags) ? ONLINE : OFFLINE;
  OnlineState old_state;
  {
    base::AutoLock lock(notifier_mac->online_state_lock_);
    old_state = notifier_mac->online_state_;
    notifier_mac->online_state_ = new_state;
  }
  if (old_state != new_state)
    NotifyObserversOfOnlineStateChange();
}

}  // namespace net
