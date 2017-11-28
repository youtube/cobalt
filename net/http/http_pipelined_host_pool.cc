// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_pool.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "net/http/http_pipelined_host_capability.h"
#include "net/http/http_pipelined_host_forced.h"
#include "net/http/http_pipelined_host_impl.h"
#include "net/http/http_server_properties.h"

using base::ListValue;
using base::Value;

namespace net {

class HttpPipelinedHostImplFactory : public HttpPipelinedHost::Factory {
 public:
  virtual HttpPipelinedHost* CreateNewHost(
      HttpPipelinedHost::Delegate* delegate,
      const HttpPipelinedHost::Key& key,
      HttpPipelinedConnection::Factory* factory,
      HttpPipelinedHostCapability capability,
      bool force_pipelining) override {
    if (force_pipelining) {
      return new HttpPipelinedHostForced(delegate, key, factory);
    } else {
      return new HttpPipelinedHostImpl(delegate, key, factory, capability);
    }
  }
};

HttpPipelinedHostPool::HttpPipelinedHostPool(
    Delegate* delegate,
    HttpPipelinedHost::Factory* factory,
    HttpServerProperties* http_server_properties,
    bool force_pipelining)
    : delegate_(delegate),
      factory_(factory),
      http_server_properties_(http_server_properties),
      force_pipelining_(force_pipelining) {
  if (!factory) {
    factory_.reset(new HttpPipelinedHostImplFactory);
  }
}

HttpPipelinedHostPool::~HttpPipelinedHostPool() {
  CHECK(host_map_.empty());
}

bool HttpPipelinedHostPool::IsKeyEligibleForPipelining(
    const HttpPipelinedHost::Key& key) {
  HttpPipelinedHostCapability capability =
      http_server_properties_->GetPipelineCapability(key.origin());
  return capability != PIPELINE_INCAPABLE;
}

HttpPipelinedStream* HttpPipelinedHostPool::CreateStreamOnNewPipeline(
    const HttpPipelinedHost::Key& key,
    ClientSocketHandle* connection,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated,
    NextProto protocol_negotiated) {
  HttpPipelinedHost* host = GetPipelinedHost(key, true);
  if (!host) {
    return NULL;
  }
  return host->CreateStreamOnNewPipeline(connection, used_ssl_config,
                                         used_proxy_info, net_log,
                                         was_npn_negotiated,
                                         protocol_negotiated);
}

HttpPipelinedStream* HttpPipelinedHostPool::CreateStreamOnExistingPipeline(
    const HttpPipelinedHost::Key& key) {
  HttpPipelinedHost* host = GetPipelinedHost(key, false);
  if (!host) {
    return NULL;
  }
  return host->CreateStreamOnExistingPipeline();
}

bool HttpPipelinedHostPool::IsExistingPipelineAvailableForKey(
    const HttpPipelinedHost::Key& key) {
  HttpPipelinedHost* host = GetPipelinedHost(key, false);
  if (!host) {
    return false;
  }
  return host->IsExistingPipelineAvailable();
}

HttpPipelinedHost* HttpPipelinedHostPool::GetPipelinedHost(
    const HttpPipelinedHost::Key& key, bool create_if_not_found) {
  HostMap::iterator host_it = host_map_.find(key);
  if (host_it != host_map_.end()) {
    CHECK(host_it->second);
    return host_it->second;
  } else if (!create_if_not_found) {
    return NULL;
  }

  HttpPipelinedHostCapability capability =
      http_server_properties_->GetPipelineCapability(key.origin());
  if (capability == PIPELINE_INCAPABLE) {
    return NULL;
  }

  HttpPipelinedHost* host = factory_->CreateNewHost(
      this, key, NULL, capability, force_pipelining_);
  host_map_[key] = host;
  return host;
}

void HttpPipelinedHostPool::OnHostIdle(HttpPipelinedHost* host) {
  const HttpPipelinedHost::Key& key = host->GetKey();
  CHECK(ContainsKey(host_map_, key));
  host_map_.erase(key);
  delete host;
}

void HttpPipelinedHostPool::OnHostHasAdditionalCapacity(
    HttpPipelinedHost* host) {
  delegate_->OnHttpPipelinedHostHasAdditionalCapacity(host);
}

void HttpPipelinedHostPool::OnHostDeterminedCapability(
    HttpPipelinedHost* host,
    HttpPipelinedHostCapability capability) {
  http_server_properties_->SetPipelineCapability(host->GetKey().origin(),
                                                 capability);
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
