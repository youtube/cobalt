// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver.h"

#include "net/base/host_cache.h"
#include "net/base/host_resolver_impl.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"

namespace net {

namespace {

// Maximum of 6 concurrent resolver threads (excluding retries).
// Some routers (or resolvers) appear to start to provide host-not-found if
// too many simultaneous resolutions are pending.  This number needs to be
// further optimized, but 8 is what FF currently does. We found some routers
// that limit this to 6, so we're temporarily holding it at that level.
const size_t kDefaultMaxProcTasks = 6u;

}  // namespace

HostResolver::Options::Options()
    : max_concurrent_resolves(kDefaultParallelism),
      max_retry_attempts(kDefaultRetryAttempts),
      enable_caching(true),
      enable_async(false) {
}

HostResolver::RequestInfo::RequestInfo(const HostPortPair& host_port_pair)
    : host_port_pair_(host_port_pair),
      address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      host_resolver_flags_(0),
      allow_cached_response_(true),
      is_speculative_(false),
      priority_(MEDIUM) {
}

HostResolver::~HostResolver() {
}

AddressFamily HostResolver::GetDefaultAddressFamily() const {
  return ADDRESS_FAMILY_UNSPECIFIED;
}

void HostResolver::ProbeIPv6Support() {
}

HostCache* HostResolver::GetHostCache() {
  return NULL;
}

base::Value* HostResolver::GetDnsConfigAsValue() const {
  return NULL;
}

// static
scoped_ptr<HostResolver>
HostResolver::CreateSystemResolver(const Options& options, NetLog* net_log) {
  size_t max_concurrent_resolves = options.max_concurrent_resolves;
  if (max_concurrent_resolves == kDefaultParallelism)
    max_concurrent_resolves = kDefaultMaxProcTasks;

  scoped_ptr<HostCache> cache;
  if (options.enable_caching)
    cache = HostCache::CreateDefaultCache();
  scoped_ptr<DnsClient> dns_client;
  if (options.enable_async) {
#if !defined(ENABLE_BUILT_IN_DNS)
    NOTREACHED();
    return scoped_ptr<HostResolver>();
#else
    dns_client = DnsClient::CreateClient(net_log);
#endif
  }

  return scoped_ptr<HostResolver>(new HostResolverImpl(
      cache.Pass(),
      PrioritizedDispatcher::Limits(NUM_PRIORITIES,
                                    max_concurrent_resolves),
      HostResolverImpl::ProcTaskParams(NULL, options.max_retry_attempts),
      dns_client.Pass(),
      net_log));
}

// static
scoped_ptr<HostResolver>
HostResolver::CreateDefaultResolver(NetLog* net_log) {
  return CreateSystemResolver(Options(), net_log);
}

HostResolver::HostResolver() {
}

}  // namespace net
