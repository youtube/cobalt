// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host.h"

#include "base/stl_util.h"
#include "net/http/http_pipelined_connection_impl.h"
#include "net/http/http_pipelined_stream.h"

namespace net {

class HttpPipelinedConnectionImplFactory :
    public HttpPipelinedConnection::Factory {
 public:
  HttpPipelinedConnection* CreateNewPipeline(
      ClientSocketHandle* connection,
      HttpPipelinedConnection::Delegate* delegate,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      const BoundNetLog& net_log,
      bool was_npn_negotiated) OVERRIDE {
    return new HttpPipelinedConnectionImpl(connection, delegate,
                                           used_ssl_config, used_proxy_info,
                                           net_log, was_npn_negotiated);
  }
};

HttpPipelinedHost::HttpPipelinedHost(
    HttpPipelinedHost::Delegate* delegate,
    const HostPortPair& origin,
    HttpPipelinedConnection::Factory* factory)
    : delegate_(delegate),
      origin_(origin),
      factory_(factory) {
  if (!factory) {
    factory_.reset(new HttpPipelinedConnectionImplFactory());
  }
}

HttpPipelinedHost::~HttpPipelinedHost() {
  CHECK(pipelines_.empty());
}

HttpPipelinedStream* HttpPipelinedHost::CreateStreamOnNewPipeline(
    ClientSocketHandle* connection,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated) {
  HttpPipelinedConnection* pipeline = factory_->CreateNewPipeline(
      connection, this, used_ssl_config, used_proxy_info, net_log,
      was_npn_negotiated);
  pipelines_.insert(pipeline);
  return pipeline->CreateNewStream();
}

HttpPipelinedStream* HttpPipelinedHost::CreateStreamOnExistingPipeline() {
  HttpPipelinedConnection* available_pipeline = NULL;
  for (std::set<HttpPipelinedConnection*>::iterator it = pipelines_.begin();
       it != pipelines_.end(); ++it) {
    if ((*it)->usable() &&
        (*it)->active() &&
        (*it)->depth() < max_pipeline_depth() &&
        (!available_pipeline || (*it)->depth() < available_pipeline->depth())) {
      available_pipeline = *it;
    }
  }
  if (!available_pipeline) {
    return NULL;
  }
  return available_pipeline->CreateNewStream();
}

bool HttpPipelinedHost::IsExistingPipelineAvailable() {
  for (std::set<HttpPipelinedConnection*>::iterator it = pipelines_.begin();
       it != pipelines_.end(); ++it) {
    if ((*it)->usable() &&
        (*it)->active() &&
        (*it)->depth() < max_pipeline_depth()) {
      return true;
    }
  }
  return false;
}

void HttpPipelinedHost::OnPipelineEmpty(HttpPipelinedConnection* pipeline) {
  CHECK(ContainsKey(pipelines_, pipeline));
  pipelines_.erase(pipeline);
  delete pipeline;
  if (pipelines_.empty()) {
    delegate_->OnHostIdle(this);
    // WARNING: We'll probably be deleted here.
  }
}

void HttpPipelinedHost::OnPipelineHasCapacity(
    HttpPipelinedConnection* pipeline) {
  if (pipeline->usable() && pipeline->depth() < max_pipeline_depth()) {
    delegate_->OnHostHasAdditionalCapacity(this);
  }
  if (!pipeline->depth()) {
    OnPipelineEmpty(pipeline);
    // WARNING: We might be deleted here.
  }
}

}  // namespace net
