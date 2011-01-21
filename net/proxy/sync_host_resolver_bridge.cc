// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/sync_host_resolver_bridge.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace net {

// SyncHostResolverBridge::Core ----------------------------------------------

class SyncHostResolverBridge::Core
    : public base::RefCountedThreadSafe<SyncHostResolverBridge::Core> {
 public:
  Core(HostResolver* resolver, MessageLoop* host_resolver_loop);

  int ResolveSynchronously(const HostResolver::RequestInfo& info,
                           AddressList* addresses);

  // Returns true if Shutdown() has been called.
  bool HasShutdown() const {
    base::AutoLock l(lock_);
    return HasShutdownLocked();
  }

  // Called on |host_resolver_loop_|.
  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<SyncHostResolverBridge::Core>;

  bool HasShutdownLocked() const {
    return has_shutdown_;
  }

  // Called on |host_resolver_loop_|.
  void StartResolve(const HostResolver::RequestInfo& info,
                    AddressList* addresses);

  // Called on |host_resolver_loop_|.
  void OnResolveCompletion(int result);

  // Not called on |host_resolver_loop_|.
  int WaitForResolveCompletion();

  HostResolver* const host_resolver_;
  MessageLoop* const host_resolver_loop_;
  net::CompletionCallbackImpl<Core> callback_;
  // The result from the current request (set on |host_resolver_loop_|).
  int err_;
  // The currently outstanding request to |host_resolver_|, or NULL.
  HostResolver::RequestHandle outstanding_request_;

  // Event to notify completion of resolve request.  We always Signal() on
  // |host_resolver_loop_| and Wait() on a different thread.
  base::WaitableEvent event_;

  // True if Shutdown() has been called. Must hold |lock_| to access it.
  bool has_shutdown_;

  // Mutex to guard accesses to |has_shutdown_|.
      mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

SyncHostResolverBridge::Core::Core(HostResolver* host_resolver,
                                   MessageLoop* host_resolver_loop)
    : host_resolver_(host_resolver),
      host_resolver_loop_(host_resolver_loop),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &Core::OnResolveCompletion)),
      err_(0),
      outstanding_request_(NULL),
      event_(true, false),
      has_shutdown_(false) {}

int SyncHostResolverBridge::Core::ResolveSynchronously(
    const HostResolver::RequestInfo& info,
    net::AddressList* addresses) {
  // Otherwise start an async resolve on the resolver's thread.
  host_resolver_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &Core::StartResolve,
                        info, addresses));

  return WaitForResolveCompletion();
}

void SyncHostResolverBridge::Core::StartResolve(
    const HostResolver::RequestInfo& info,
    net::AddressList* addresses) {
  DCHECK_EQ(MessageLoop::current(), host_resolver_loop_);
  DCHECK(!outstanding_request_);

  if (HasShutdown())
    return;

  int error = host_resolver_->Resolve(
      info, addresses, &callback_, &outstanding_request_, BoundNetLog());
  if (error != ERR_IO_PENDING)
    OnResolveCompletion(error);  // Completed synchronously.
}

void SyncHostResolverBridge::Core::OnResolveCompletion(int result) {
  DCHECK_EQ(MessageLoop::current(), host_resolver_loop_);
  err_ = result;
  outstanding_request_ = NULL;
  event_.Signal();
}

int SyncHostResolverBridge::Core::WaitForResolveCompletion() {
  DCHECK_NE(MessageLoop::current(), host_resolver_loop_);
  event_.Wait();

  {
    base::AutoLock l(lock_);
    if (HasShutdownLocked())
      return ERR_ABORTED;
    event_.Reset();
  }

  return err_;
}

void SyncHostResolverBridge::Core::Shutdown() {
  DCHECK_EQ(MessageLoop::current(), host_resolver_loop_);

  if (outstanding_request_) {
    host_resolver_->CancelRequest(outstanding_request_);
    outstanding_request_ = NULL;
  }

  {
    base::AutoLock l(lock_);
    has_shutdown_ = true;
  }

  // Wake up the PAC thread in case it was waiting for resolve completion.
  event_.Signal();
}

// SyncHostResolverBridge -----------------------------------------------------

SyncHostResolverBridge::SyncHostResolverBridge(HostResolver* host_resolver,
                                               MessageLoop* host_resolver_loop)
    : host_resolver_loop_(host_resolver_loop),
      core_(new Core(host_resolver, host_resolver_loop)) {
  DCHECK(host_resolver_loop_);
}

SyncHostResolverBridge::~SyncHostResolverBridge() {
  DCHECK(core_->HasShutdown());
}

int SyncHostResolverBridge::Resolve(const RequestInfo& info,
                                    AddressList* addresses,
                                    CompletionCallback* callback,
                                    RequestHandle* out_req,
                                    const BoundNetLog& net_log) {
  DCHECK(!callback);
  DCHECK(!out_req);

  return core_->ResolveSynchronously(info, addresses);
}

void SyncHostResolverBridge::CancelRequest(RequestHandle req) {
  NOTREACHED();
}

void SyncHostResolverBridge::AddObserver(Observer* observer) {
  NOTREACHED();
}

void SyncHostResolverBridge::RemoveObserver(Observer* observer) {
  NOTREACHED();
}

void SyncHostResolverBridge::Shutdown() {
  DCHECK_EQ(MessageLoop::current(), host_resolver_loop_);
  core_->Shutdown();
}

}  // namespace net
