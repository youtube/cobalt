// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_SYNC_HOST_RESOLVER_H_
#define NET_PROXY_SYNC_HOST_RESOLVER_H_
#pragma once

#include "net/base/host_resolver.h"
#include "net/base/net_export.h"

namespace net {

// Interface used by ProxyResolverJSBindings to abstract a synchronous host
// resolver module (which includes a Shutdown method).
class NET_EXPORT_PRIVATE SyncHostResolver {
 public:
  virtual ~SyncHostResolver() {}

  virtual int Resolve(const HostResolver::RequestInfo& info,
                      AddressList* addresses) = 0;

  // Optionally aborts any blocking resolves that are in progress.
  virtual void Shutdown() = 0;
};

}  // namespace net

#endif  // NET_PROXY_SYNC_HOST_RESOLVER_H_
