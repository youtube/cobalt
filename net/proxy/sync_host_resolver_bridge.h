// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_SYNC_HOST_RESOLVER_BRIDGE_H_
#define NET_PROXY_SYNC_HOST_RESOLVER_BRIDGE_H_

#include "base/scoped_ptr.h"
#include "net/base/host_resolver.h"
#include "net/proxy/single_threaded_proxy_resolver.h"

class MessageLoop;

namespace net {

// Wrapper around HostResolver to give a sync API while running the resolver
// in async mode on |host_resolver_loop|.
class SyncHostResolverBridge : public HostResolver {
 public:
  SyncHostResolverBridge(HostResolver* host_resolver,
                         MessageLoop* host_resolver_loop);

  virtual ~SyncHostResolverBridge();

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log);
  virtual void CancelRequest(RequestHandle req);
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // The Shutdown() method should be called prior to destruction, from
  // |host_resolver_loop_|. It aborts any in progress synchronous resolves, to
  // prevent deadlocks from happening.
  void Shutdown();

 private:
  class Core;

  MessageLoop* const host_resolver_loop_;
  scoped_refptr<Core> core_;
};

// Subclass of SingleThreadedProxyResolver that additionally calls
// |bridged_host_resolver_->Shutdown()| during its destructor.
class SingleThreadedProxyResolverUsingBridgedHostResolver
    : public SingleThreadedProxyResolver {
 public:
  SingleThreadedProxyResolverUsingBridgedHostResolver(
      ProxyResolver* proxy_resolver,
      SyncHostResolverBridge* bridged_host_resolver);

  virtual ~SingleThreadedProxyResolverUsingBridgedHostResolver();

 private:
  scoped_refptr<SyncHostResolverBridge> bridged_host_resolver_;
};


}  // namespace net

#endif  // NET_PROXY_SYNC_HOST_RESOLVER_BRIDGE_H_
