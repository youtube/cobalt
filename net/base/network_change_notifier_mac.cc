// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_mac.h"

#include <SystemConfiguration/SCDynamicStoreKey.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#include <algorithm>

#include "base/thread.h"

// We only post tasks to a child thread we own, so we don't need refcounting.
DISABLE_RUNNABLE_METHOD_REFCOUNT(net::NetworkChangeNotifierMac);

namespace net {

NetworkChangeNotifierMac::NetworkChangeNotifierMac()
    : notifier_thread_(new base::Thread("NetworkChangeNotifier")) {
  // We create this notifier thread because the notification implementation
  // needs a thread with a CFRunLoop, and there's no guarantee that
  // MessageLoop::current() meets that criterion.
  base::Thread::Options thread_options(MessageLoop::TYPE_UI, 0);
  notifier_thread_->StartWithOptions(thread_options);
  // TODO(willchan): Look to see if there's a better signal for when it's ok to
  // initialize this, rather than just delaying it by a fixed time.
  const int kNotifierThreadInitializationDelayMS = 1000;
  notifier_thread_->message_loop()->PostDelayedTask(FROM_HERE,
      NewRunnableMethod(this, &NetworkChangeNotifierMac::Init),
      kNotifierThreadInitializationDelayMS);
}

NetworkChangeNotifierMac::~NetworkChangeNotifierMac() {
  // We don't need to explicitly Stop(), but doing so allows us to sanity-
  // check that the notifier thread shut down properly.
  notifier_thread_->Stop();
  DCHECK(run_loop_source_ == NULL);
}

// static
void NetworkChangeNotifierMac::DynamicStoreCallback(
    SCDynamicStoreRef /* store */,
    CFArrayRef changed_keys,
    void* config) {
  NetworkChangeNotifierMac* net_config =
      static_cast<NetworkChangeNotifierMac*>(config);
  net_config->OnNetworkConfigChange(changed_keys);
}

void NetworkChangeNotifierMac::WillDestroyCurrentMessageLoop() {
  DCHECK(notifier_thread_ != NULL);
  // We can't check the notifier_thread_'s message_loop(), as it's now 0.
  // DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  DCHECK(run_loop_source_ != NULL);
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), run_loop_source_.get(),
                        kCFRunLoopCommonModes);
  run_loop_source_.reset();
}

void NetworkChangeNotifierMac::Init() {
  DCHECK(notifier_thread_ != NULL);
  DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  // Add a run loop source for a dynamic store to the current run loop.
  SCDynamicStoreContext context = {
    0,     // Version 0.
    this,  // User data.
    NULL,  // This is not reference counted.  No retain function.
    NULL,  // This is not reference counted.  No release function.
    NULL,  // No description for this.
  };
  scoped_cftyperef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
      NULL, CFSTR("org.chromium"), DynamicStoreCallback, &context));
  run_loop_source_.reset(SCDynamicStoreCreateRunLoopSource(
      NULL, store.get(), 0));
  CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source_.get(),
                     kCFRunLoopCommonModes);

  // Set up notifications for interface and IP address changes.
  scoped_cftyperef<CFMutableArrayRef> notification_keys(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  scoped_cftyperef<CFStringRef> key(SCDynamicStoreKeyCreateNetworkGlobalEntity(
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
      store.get(), notification_keys.get(), NULL);
  // TODO(willchan): Figure out a proper way to handle this rather than crash.
  CHECK(ret);

  MessageLoop::current()->AddDestructionObserver(this);
}

void NetworkChangeNotifierMac::OnNetworkConfigChange(CFArrayRef changed_keys) {
  DCHECK(notifier_thread_ != NULL);
  DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

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

}  // namespace net
