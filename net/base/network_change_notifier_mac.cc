// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// There are three classes involved here.  There's NetworkChangeNotifierMac,
// which is the Mac specific implementation of NetworkChangeNotifier.  It is the
// class with which clients can register themselves as network change 
// observers.  There's NetworkChangeNotifierThread, which is a base::Thread
// subclass of MessageLoop::TYPE_UI (since it needs a CFRunLoop) that contains
// the NetworkChangeNotifierImpl.  NetworkChangeNotifierImpl is the object
// that receives the actual OS X notifications and posts them to the
// NetworkChangeNotifierMac's message loop, so that NetworkChangeNotifierMac
// can notify all its observers.
//
// When NetworkChangeNotifierMac is being deleted, it will delete the
// NetworkChangeNotifierThread, which will Stop() it and also delete the
// NetworkChangeNotifierImpl.  Therefore, NetworkChangeNotifierImpl and
// NetworkChangeNotifierThread's lifetimes generally begin after and end before
// NetworkChangeNotifierMac.  There is an edge case where a notification task
// gets posted to the IO thread, thereby maintaining a reference to
// NetworkChangeNotifierImpl beyond the lifetime of NetworkChangeNotifierThread.
// In this case, the notification is cancelled, and NetworkChangeNotifierImpl
// will be deleted once all notification tasks that reference it have been run.

#include "net/base/network_change_notifier_mac.h"
#include <SystemConfiguration/SCDynamicStore.h>
#include <SystemConfiguration/SCDynamicStoreKey.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#include <algorithm>
#include "base/logging.h"
#include "base/scoped_cftyperef.h"
#include "base/thread.h"

namespace net {

namespace {

// NetworkChangeNotifierImpl should be created on a thread with a CFRunLoop,
// since it requires one to pump notifications.  However, it also runs some
// methods on |notifier_loop_|, because it cannot post calls to |notifier_|
// since NetworkChangeNotifier is not ref counted in a thread safe manner.
class NetworkChangeNotifierImpl
    : public base::RefCountedThreadSafe<NetworkChangeNotifierImpl> {
 public:
  NetworkChangeNotifierImpl(MessageLoop* notifier_loop,
                            NetworkChangeNotifierMac* notifier);

  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<NetworkChangeNotifierImpl>;
  ~NetworkChangeNotifierImpl();

  static void DynamicStoreCallback(SCDynamicStoreRef /* store */,
                                   CFArrayRef changed_keys,
                                   void* config);

  void OnNetworkConfigChange(CFArrayRef changed_keys);

  // Runs on |notifier_loop_|.
  void OnIPAddressChanged();

  // Raw pointers.  Note that |notifier_| _must_ outlive the
  // NetworkChangeNotifierImpl.  For lifecycle management details, read the
  // comment at the top of the file.
  MessageLoop* const notifier_loop_;
  NetworkChangeNotifierMac* notifier_;

  scoped_cftyperef<CFRunLoopSourceRef> source_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierImpl);
};

NetworkChangeNotifierImpl::NetworkChangeNotifierImpl(
    MessageLoop* notifier_loop, NetworkChangeNotifierMac* notifier)
    : notifier_loop_(notifier_loop),
      notifier_(notifier) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  SCDynamicStoreContext context = {
    0,    // Version 0.
    this, // User data.
    NULL, // This is not reference counted.  No retain function.
    NULL, // This is not reference counted.  No release function.
    NULL, // No description for this.
  };

  // Get a reference to the dynamic store.
  scoped_cftyperef<SCDynamicStoreRef> store(
      SCDynamicStoreCreate(NULL /* use default allocator */,
                           CFSTR("org.chromium"),
                           DynamicStoreCallback, &context));

  // Create a run loop source for the dynamic store.
  source_.reset(SCDynamicStoreCreateRunLoopSource(
      NULL /* use default allocator */,
      store.get(),
      0 /* 0 sounds like a fine source order to me! */));

  // Add the run loop source to the current run loop.
  CFRunLoopAddSource(CFRunLoopGetCurrent(),
                     source_.get(),
                     kCFRunLoopCommonModes);

  // Set up the notification keys.
  scoped_cftyperef<CFMutableArrayRef> notification_keys(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));

  // Monitor interface changes.
  scoped_cftyperef<CFStringRef> key(
      SCDynamicStoreKeyCreateNetworkGlobalEntity(
          NULL /* default allocator */, kSCDynamicStoreDomainState,
          kSCEntNetInterface));
  CFArrayAppendValue(notification_keys.get(), key.get());

  // Monitor IP address changes.

  key.reset(SCDynamicStoreKeyCreateNetworkGlobalEntity(
      NULL /* default allocator */, kSCDynamicStoreDomainState,
      kSCEntNetIPv4));
  CFArrayAppendValue(notification_keys.get(), key.get());

  key.reset(SCDynamicStoreKeyCreateNetworkGlobalEntity(
      NULL /* default allocator */, kSCDynamicStoreDomainState,
      kSCEntNetIPv6));
  CFArrayAppendValue(notification_keys.get(), key.get());

  // Ok, let's ask for notifications!
  // TODO(willchan): Figure out a proper way to handle this rather than crash.
  CHECK(SCDynamicStoreSetNotificationKeys(
      store.get(), notification_keys.get(), NULL));
}

NetworkChangeNotifierImpl::~NetworkChangeNotifierImpl() {
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                        source_.get(),
                        kCFRunLoopCommonModes);
}

void NetworkChangeNotifierImpl::Shutdown() {
  CHECK(notifier_);
  notifier_ = NULL;
}

// static
void NetworkChangeNotifierImpl::DynamicStoreCallback(
    SCDynamicStoreRef /* store */,
    CFArrayRef changed_keys,
    void* config) {
  NetworkChangeNotifierImpl* net_config =
      static_cast<NetworkChangeNotifierImpl*>(config);
  net_config->OnNetworkConfigChange(changed_keys);
}

void NetworkChangeNotifierImpl::OnNetworkConfigChange(CFArrayRef changed_keys) {
  for (CFIndex i = 0; i < CFArrayGetCount(changed_keys); ++i) {
    CFStringRef key = static_cast<CFStringRef>(
        CFArrayGetValueAtIndex(changed_keys, i));
    if (CFStringHasSuffix(key, kSCEntNetIPv4) ||
        CFStringHasSuffix(key, kSCEntNetIPv6)) {
      notifier_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(
              this,
              &NetworkChangeNotifierImpl::OnIPAddressChanged));
    } else if (CFStringHasSuffix(key, kSCEntNetInterface)) {
      // TODO(willchan): Does not appear to be working.  Look into this.
      // Perhaps this isn't needed anyway.
    } else {
      NOTREACHED();
    }
  }
}

void NetworkChangeNotifierImpl::OnIPAddressChanged() {
  // If |notifier_| doesn't exist, then that means we're shutting down, so
  // notifications are all cancelled.
  if (notifier_)
    notifier_->OnIPAddressChanged();
}

class NetworkChangeNotifierThread : public base::Thread {
 public:
  NetworkChangeNotifierThread(MessageLoop* notifier_loop,
                              NetworkChangeNotifierMac* notifier);
  ~NetworkChangeNotifierThread();

 protected:
  virtual void Init();

 private:
  MessageLoop* const notifier_loop_;
  NetworkChangeNotifierMac* const notifier_;
  scoped_refptr<NetworkChangeNotifierImpl> notifier_impl_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierThread);
};

NetworkChangeNotifierThread::NetworkChangeNotifierThread(
    MessageLoop* notifier_loop, NetworkChangeNotifierMac* notifier)
    : base::Thread("NetworkChangeNotifier"),
      notifier_loop_(notifier_loop),
      notifier_(notifier) {}

NetworkChangeNotifierThread::~NetworkChangeNotifierThread() {
  notifier_impl_->Shutdown();
}

// Note that |notifier_impl_| is initialized on the network change
// notifier thread, not whatever thread constructs the
// NetworkChangeNotifierThread object.  This is important, because this thread
// is the one that has a CFRunLoop.
void NetworkChangeNotifierThread::Init() {
  notifier_impl_ =
      new NetworkChangeNotifierImpl(notifier_loop_, notifier_);
}

}  // namespace

NetworkChangeNotifierMac::NetworkChangeNotifierMac()
    : notifier_thread_(
          new NetworkChangeNotifierThread(MessageLoop::current(), this)) {
  base::Thread::Options thread_options;
  thread_options.message_loop_type = MessageLoop::TYPE_UI;
  notifier_thread_->StartWithOptions(thread_options);
}

NetworkChangeNotifierMac::~NetworkChangeNotifierMac() {}

}  // namespace net
