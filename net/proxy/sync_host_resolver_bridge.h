// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_SYNC_HOST_RESOLVER_BRIDGE_H_
#define NET_PROXY_SYNC_HOST_RESOLVER_BRIDGE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/proxy/sync_host_resolver.h"

class MessageLoop;

namespace net {

// Wrapper around HostResolver to give a sync API while running the resolver
// in async mode on |host_resolver_loop|.
class NET_EXPORT_PRIVATE SyncHostResolverBridge : public SyncHostResolver {
 public:
  SyncHostResolverBridge(HostResolver* host_resolver,
                         MessageLoop* host_resolver_loop);

  virtual ~SyncHostResolverBridge();

  // SyncHostResolver methods:
  virtual int Resolve(const HostResolver::RequestInfo& info,
                      AddressList* addresses,
                      const BoundNetLog& net_log) OVERRIDE;

  // The Shutdown() method should be called prior to destruction, from
  // |host_resolver_loop_|. It aborts any in progress synchronous resolves, to
  // prevent deadlocks from happening.
  virtual void Shutdown() OVERRIDE;

 private:
  class Core;

  MessageLoop* const host_resolver_loop_;
  scoped_refptr<Core> core_;
  DISALLOW_COPY_AND_ASSIGN(SyncHostResolverBridge);
};

}  // namespace net

#endif  // NET_PROXY_SYNC_HOST_RESOLVER_BRIDGE_H_
