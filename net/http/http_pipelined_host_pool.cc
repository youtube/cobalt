// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_pool.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/http/http_pipelined_host_capability.h"
#include "net/http/http_pipelined_host_impl.h"
#include "net/http/http_server_properties.h"

using base::ListValue;
using base::Value;

namespace net {

class HttpPipelinedHostImplFactory : public HttpPipelinedHost::Factory {
 public:
  virtual HttpPipelinedHost* CreateNewHost(
      HttpPipelinedHost::Delegate* delegate, const HostPortPair& origin,
      HttpPipelinedConnection::Factory* factory,
      HttpPipelinedHostCapability capability) OVERRIDE {
    return new HttpPipelinedHostImpl(delegate, origin, factory, capability);
  }
};

HttpPipelinedHostPool::HttpPipelinedHostPool(
    Delegate* delegate,
    HttpPipelinedHost::Factory* factory,
    HttpServerProperties* http_server_properties)
    : delegate_(delegate),
      factory_(factory),
      http_server_properties_(http_server_properties) {
  if (!factory) {
    factory_.reset(new HttpPipelinedHostImplFactory);
  }
}

HttpPipelinedHostPool::~HttpPipelinedHostPool() {
  CHECK(host_map_.empty());
}

bool HttpPipelinedHostPool::IsHostEligibleForPipelining(
    const HostPortPair& origin) {
  HttpPipelinedHostCapability capability =
      http_server_properties_->GetPipelineCapability(origin);
  return capability != PIPELINE_INCAPABLE;
}

HttpPipelinedStream* HttpPipelinedHostPool::CreateStreamOnNewPipeline(
    const HostPortPair& origin,
    ClientSocketHandle* connection,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated,
    SSLClientSocket::NextProto protocol_negotiated) {
  HttpPipelinedHost* host = GetPipelinedHost(origin, true);
  if (!host) {
    return NULL;
  }
  return host->CreateStreamOnNewPipeline(connection, used_ssl_config,
                                         used_proxy_info, net_log,
                                         was_npn_negotiated,
                                         protocol_negotiated);
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

  HttpPipelinedHostCapability capability =
      http_server_properties_->GetPipelineCapability(origin);
  if (capability == PIPELINE_INCAPABLE) {
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
    HttpPipelinedHostCapability capability) {
  http_server_properties_->SetPipelineCapability(host->origin(), capability);
}

Value* HttpPipelinedHostPool::PipelineInfoToValue() const {
  ListValue* list = new ListValue();
  for (HostMap::const_iterator it = host_map_.begin();
       it != host_map_.end(); ++it) {
    Value* value = it->second->PipelineInfoToValue();
    list->Append(value);
  }
  return list;
}

}  // namespace net
