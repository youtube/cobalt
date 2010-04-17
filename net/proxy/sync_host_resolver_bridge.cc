// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/sync_host_resolver_bridge.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

namespace net {

// SyncHostResolverBridge -----------------------------------------------------

SyncHostResolverBridge::SyncHostResolverBridge(HostResolver* host_resolver,
                                               MessageLoop* host_resolver_loop)
    : host_resolver_(host_resolver),
      host_resolver_loop_(host_resolver_loop),
      event_(true, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &SyncHostResolverBridge::OnResolveCompletion)),
      outstanding_request_(NULL),
      has_shutdown_(false) {
  DCHECK(host_resolver_loop_);
}

SyncHostResolverBridge::~SyncHostResolverBridge() {
  DCHECK(HasShutdown());
}

int SyncHostResolverBridge::Resolve(const RequestInfo& info,
                                    AddressList* addresses,
                                    CompletionCallback* callback,
                                    RequestHandle* out_req,
                                    const BoundNetLog& net_log) {
  DCHECK(!callback);
  DCHECK(!out_req);

  // Otherwise start an async resolve on the resolver's thread.
  host_resolver_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &SyncHostResolverBridge::StartResolve,
                        info, addresses));

  // Wait for the resolve to complete in the resolver's thread.
  event_.Wait();

  {
    AutoLock l(lock_);
    if (has_shutdown_)
      return ERR_ABORTED;
    event_.Reset();
  }

  return err_;
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

  if (outstanding_request_) {
    host_resolver_->CancelRequest(outstanding_request_);
    outstanding_request_ = NULL;
  }

  AutoLock l(lock_);
  has_shutdown_ = true;

  // Wake up the PAC thread in case it was waiting for resolve completion.
  event_.Signal();
}

void SyncHostResolverBridge::StartResolve(const HostResolver::RequestInfo& info,
                                          net::AddressList* addresses) {
  DCHECK_EQ(host_resolver_loop_, MessageLoop::current());
  DCHECK(!outstanding_request_);

  if (HasShutdown())
    return;

  int error = host_resolver_->Resolve(
      info, addresses, &callback_, &outstanding_request_, BoundNetLog());
  if (error != ERR_IO_PENDING)
    OnResolveCompletion(error);  // Completed synchronously.
}

void SyncHostResolverBridge::OnResolveCompletion(int result) {
  DCHECK_EQ(host_resolver_loop_, MessageLoop::current());
  err_ = result;
  outstanding_request_ = NULL;
  event_.Signal();
}

// SingleThreadedProxyResolverUsingBridgedHostResolver -----------------------

SingleThreadedProxyResolverUsingBridgedHostResolver::
SingleThreadedProxyResolverUsingBridgedHostResolver(
    ProxyResolver* proxy_resolver,
    SyncHostResolverBridge* bridged_host_resolver)
    : SingleThreadedProxyResolver(proxy_resolver),
      bridged_host_resolver_(bridged_host_resolver) {
}

SingleThreadedProxyResolverUsingBridgedHostResolver::
~SingleThreadedProxyResolverUsingBridgedHostResolver() {
  bridged_host_resolver_->Shutdown();
}

}  // namespace net
