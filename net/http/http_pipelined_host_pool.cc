// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_pool.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/http/http_pipelined_host_impl.h"

namespace net {

// TODO(simonjam): Run experiments with different values of this to see what
// value is good at avoiding evictions without eating too much memory. Until
// then, this is just a bad guess.
static const int kNumHostsToRemember = 200;

class HttpPipelinedHostImplFactory : public HttpPipelinedHost::Factory {
 public:
  virtual HttpPipelinedHost* CreateNewHost(
      HttpPipelinedHost::Delegate* delegate, const HostPortPair& origin,
      HttpPipelinedConnection::Factory* factory,
      HttpPipelinedHost::Capability capability) OVERRIDE {
    return new HttpPipelinedHostImpl(delegate, origin, factory, capability);
  }
};

HttpPipelinedHostPool::HttpPipelinedHostPool(
    Delegate* delegate,
    HttpPipelinedHost::Factory* factory)
    : delegate_(delegate),
      factory_(factory),
      known_capability_map_(kNumHostsToRemember) {
  if (!factory) {
    factory_.reset(new HttpPipelinedHostImplFactory);
  }
}

HttpPipelinedHostPool::~HttpPipelinedHostPool() {
  CHECK(host_map_.empty());
}

bool HttpPipelinedHostPool::IsHostEligibleForPipelining(
    const HostPortPair& origin) {
  HttpPipelinedHost::Capability capability = GetHostCapability(origin);
  return capability != HttpPipelinedHost::INCAPABLE;
}

HttpPipelinedStream* HttpPipelinedHostPool::CreateStreamOnNewPipeline(
    const HostPortPair& origin,
    ClientSocketHandle* connection,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated) {
  HttpPipelinedHost* host = GetPipelinedHost(origin, true);
  if (!host) {
    return NULL;
  }
  return host->CreateStreamOnNewPipeline(connection, used_ssl_config,
                                         used_proxy_info, net_log,
                                         was_npn_negotiated);
}

HttpPipelinedStream* HttpPipelinedHostPool::CreateStreamOnExistingPipeline(
    const HostPortPair& origin) {
  HttpPipelinedHost* host = GetPipelinedHost(origin, false);
  if (!host) {
    return NULL;
  }
  return host->CreateStreamOnExistingPipeline();
}

bool HttpPipelinedHostPool::IsExistingPipelineAvailableForOrigin(
    const HostPortPair& origin) {
  HttpPipelinedHost* host = GetPipelinedHost(origin, false);
  if (!host) {
    return false;
  }
  return host->IsExistingPipelineAvailable();
}

HttpPipelinedHost* HttpPipelinedHostPool::GetPipelinedHost(
    const HostPortPair& origin, bool create_if_not_found) {
  HostMap::iterator host_it = host_map_.find(origin);
  if (host_it != host_map_.end()) {
    CHECK(host_it->second);
    return host_it->second;
  } else if (!create_if_not_found) {
    return NULL;
  }

  HttpPipelinedHost::Capability capability = GetHostCapability(origin);
  if (capability == HttpPipelinedHost::INCAPABLE) {
    return NULL;
  }

  HttpPipelinedHost* host = factory_->CreateNewHost(
      this, origin, NULL, capability);
  host_map_[origin] = host;
  return host;
}

void HttpPipelinedHostPool::OnHostIdle(HttpPipelinedHost* host) {
  const HostPortPair& origin = host->origin();
  CHECK(ContainsKey(host_map_, origin));
  host_map_.erase(origin);
  delete host;
}

void HttpPipelinedHostPool::OnHostHasAdditionalCapacity(
    HttpPipelinedHost* host) {
  delegate_->OnHttpPipelinedHostHasAdditionalCapacity(host->origin());
}

void HttpPipelinedHostPool::OnHostDeterminedCapability(
    HttpPipelinedHost* host,
    HttpPipelinedHost::Capability capability) {
  CapabilityMap::iterator known_it = known_capability_map_.Get(host->origin());
  if (known_it == known_capability_map_.end() ||
      known_it->second != HttpPipelinedHost::INCAPABLE) {
    known_capability_map_.Put(host->origin(), capability);
  }
}

HttpPipelinedHost::Capability HttpPipelinedHostPool::GetHostCapability(
    const HostPortPair& origin) {
  HttpPipelinedHost::Capability capability = HttpPipelinedHost::UNKNOWN;
  CapabilityMap::const_iterator it = known_capability_map_.Get(origin);
  if (it != known_capability_map_.end()) {
    capability = it->second;
  }
  return capability;
}

}  // namespace net
