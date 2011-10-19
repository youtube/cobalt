// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_pool.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/http/http_pipelined_host.h"
#include "net/http/http_stream_factory_impl.h"

namespace net {

HttpPipelinedHostPool::HttpPipelinedHostPool(HttpStreamFactoryImpl* factory)
    : factory_(factory) {
}

HttpPipelinedHostPool::~HttpPipelinedHostPool() {
  CHECK(host_map_.empty());
}

HttpPipelinedStream* HttpPipelinedHostPool::CreateStreamOnNewPipeline(
    const HostPortPair& origin,
    ClientSocketHandle* connection,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated) {
  HttpPipelinedHost* host = GetPipelinedHost(origin, true);
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
  HostMap::iterator it = host_map_.find(origin);
  if (it != host_map_.end()) {
    CHECK(it->second);
    return it->second;
  } else if (!create_if_not_found) {
    return NULL;
  }
  HttpPipelinedHost* host = new HttpPipelinedHost(this, origin, NULL);
  host_map_[origin] = host;
  return host;
}

void HttpPipelinedHostPool::OnHostIdle(HttpPipelinedHost* host) {
  const HostPortPair& origin = host->origin();
  CHECK(ContainsKey(host_map_, origin));
  // TODO(simonjam): We should remember the pipeline state for each host.
  host_map_.erase(origin);
  delete host;
}

void HttpPipelinedHostPool::OnHostHasAdditionalCapacity(
    HttpPipelinedHost* host) {
  factory_->OnHttpPipelinedHostHasAdditionalCapacity(host->origin());
}

}  // namespace net
