// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_config_watcher_mac.h"

#include <SystemConfiguration/SCDynamicStoreKey.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#include <algorithm>

#include "base/thread.h"

// We only post tasks to a child thread we own, so we don't need refcounting.
DISABLE_RUNNABLE_METHOD_REFCOUNT(net::NetworkConfigWatcherMac);

namespace net {

namespace {

// Called back by OS.  Calls OnNetworkConfigChange().
void DynamicStoreCallback(SCDynamicStoreRef /* store */,
                          CFArrayRef changed_keys,
                          void* config_delegate) {
  NetworkConfigWatcherMac::Delegate* net_config_delegate =
      static_cast<NetworkConfigWatcherMac::Delegate*>(config_delegate);
  net_config_delegate->OnNetworkConfigChange(changed_keys);
}

}  // namespace

NetworkConfigWatcherMac::NetworkConfigWatcherMac(
    Delegate* delegate)
    : notifier_thread_(new base::Thread("NetworkConfigWatcher")),
      delegate_(delegate) {
  // We create this notifier thread because the notification implementation
  // needs a thread with a CFRunLoop, and there's no guarantee that
  // MessageLoop::current() meets that criterion.
  base::Thread::Options thread_options(MessageLoop::TYPE_UI, 0);
  notifier_thread_->StartWithOptions(thread_options);
  // TODO(willchan): Look to see if there's a better signal for when it's ok to
  // initialize this, rather than just delaying it by a fixed time.
  const int kNotifierThreadInitializationDelayMS = 1000;
  notifier_thread_->message_loop()->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(this, &NetworkConfigWatcherMac::Init),
      kNotifierThreadInitializationDelayMS);
}

NetworkConfigWatcherMac::~NetworkConfigWatcherMac() {
  // We don't need to explicitly Stop(), but doing so allows us to sanity-
  // check that the notifier thread shut down properly.
  notifier_thread_->Stop();
  DCHECK(run_loop_source_ == NULL);
}

void NetworkConfigWatcherMac::WillDestroyCurrentMessageLoop() {
  DCHECK(notifier_thread_ != NULL);
  // We can't check the notifier_thread_'s message_loop(), as it's now 0.
  // DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  DCHECK(run_loop_source_ != NULL);
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), run_loop_source_.get(),
                        kCFRunLoopCommonModes);
  run_loop_source_.reset();
}

void NetworkConfigWatcherMac::Init() {
  DCHECK(notifier_thread_ != NULL);
  DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  // Add a run loop source for a dynamic store to the current run loop.
  SCDynamicStoreContext context = {
    0,          // Version 0.
    delegate_,  // User data.
    NULL,       // This is not reference counted.  No retain function.
    NULL,       // This is not reference counted.  No release function.
    NULL,       // No description for this.
  };
  scoped_cftyperef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
      NULL, CFSTR("org.chromium"), DynamicStoreCallback, &context));
  run_loop_source_.reset(SCDynamicStoreCreateRunLoopSource(
      NULL, store.get(), 0));
  CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source_.get(),
                     kCFRunLoopCommonModes);

  // Set up notifications for interface and IP address changes.
  delegate_->SetDynamicStoreNotificationKeys(store.get());

  MessageLoop::current()->AddDestructionObserver(this);
}

}  // namespace net
