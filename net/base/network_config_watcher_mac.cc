// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_config_watcher_mac.h"

#include <SystemConfiguration/SCDynamicStoreKey.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#include <algorithm>

#include "base/compiler_specific.h"
#include "base/threading/thread.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_restrictions.h"

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

class NetworkConfigWatcherMacThread : public base::Thread {
 public:
  NetworkConfigWatcherMacThread(NetworkConfigWatcherMac::Delegate* delegate);
  virtual ~NetworkConfigWatcherMacThread();

 protected:
  // base::Thread
  virtual void Init();
  virtual void CleanUp();

 private:
  // The SystemConfiguration calls in this function can lead to contention early
  // on, so we invoke this function later on in startup to keep it fast.
  void InitNotifications();

  base::mac::ScopedCFTypeRef<CFRunLoopSourceRef> run_loop_source_;
  NetworkConfigWatcherMac::Delegate* const delegate_;
  ScopedRunnableMethodFactory<NetworkConfigWatcherMacThread> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigWatcherMacThread);
};

NetworkConfigWatcherMacThread::NetworkConfigWatcherMacThread(
    NetworkConfigWatcherMac::Delegate* delegate)
    : base::Thread("NetworkConfigWatcher"),
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

NetworkConfigWatcherMacThread::~NetworkConfigWatcherMacThread() {
  // Allow IO because Stop() calls PlatformThread::Join(), which is a blocking
  // operation. This is expected during shutdown.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  Stop();
}

void NetworkConfigWatcherMacThread::Init() {
  // Disallow IO to make sure NetworkConfigWatcherMacThread's helper thread does
  // not perform blocking operations.
  base::ThreadRestrictions::SetIOAllowed(false);

  // TODO(willchan): Look to see if there's a better signal for when it's ok to
  // initialize this, rather than just delaying it by a fixed time.
  const int kInitializationDelayMS = 1000;
  message_loop()->PostDelayedTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &NetworkConfigWatcherMacThread::InitNotifications),
      kInitializationDelayMS);
}

void NetworkConfigWatcherMacThread::CleanUp() {
  if (!run_loop_source_.get())
    return;

  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), run_loop_source_.get(),
                        kCFRunLoopCommonModes);
  run_loop_source_.reset();
}

void NetworkConfigWatcherMacThread::InitNotifications() {
  // Add a run loop source for a dynamic store to the current run loop.
  SCDynamicStoreContext context = {
    0,          // Version 0.
    delegate_,  // User data.
    NULL,       // This is not reference counted.  No retain function.
    NULL,       // This is not reference counted.  No release function.
    NULL,       // No description for this.
  };
  base::mac::ScopedCFTypeRef<SCDynamicStoreRef> store(SCDynamicStoreCreate(
      NULL, CFSTR("org.chromium"), DynamicStoreCallback, &context));
  run_loop_source_.reset(SCDynamicStoreCreateRunLoopSource(
      NULL, store.get(), 0));
  CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source_.get(),
                     kCFRunLoopCommonModes);

  // Set up notifications for interface and IP address changes.
  delegate_->SetDynamicStoreNotificationKeys(store.get());
}

}  // namespace

NetworkConfigWatcherMac::NetworkConfigWatcherMac(Delegate* delegate)
    : notifier_thread_(new NetworkConfigWatcherMacThread(delegate)) {
  // We create this notifier thread because the notification implementation
  // needs a thread with a CFRunLoop, and there's no guarantee that
  // MessageLoop::current() meets that criterion.
  base::Thread::Options thread_options(MessageLoop::TYPE_UI, 0);
  notifier_thread_->StartWithOptions(thread_options);
}

NetworkConfigWatcherMac::~NetworkConfigWatcherMac() {}

}  // namespace net
