// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_REQUEST_CONTEXT_H_
#define NET_PROXY_PROXY_RESOLVER_REQUEST_CONTEXT_H_
#pragma once

namespace net {

class HostCache;
class BoundNetLog;

// This data structure holds state related to an invocation of
// "FindProxyForURL()". It is used to associate per-request
// data that can be retrieved by the bindings.
struct ProxyResolverRequestContext {
  // All of these pointers are expected to remain valid for duration of
  // this instance's lifetime.
  ProxyResolverRequestContext(const BoundNetLog* net_log,
                              HostCache* host_cache)
    : net_log(net_log),
      host_cache(host_cache) {
  }

  const BoundNetLog* net_log;
  HostCache* host_cache;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_REQUEST_CONTEXT_H_
