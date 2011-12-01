// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_impl.h"

#include "base/stl_util.h"
#include "net/http/http_pipelined_connection_impl.h"
#include "net/http/http_pipelined_stream.h"

namespace net {

// TODO(simonjam): Run experiments to see what value minimizes evictions without
// costing too much performance. Until then, this is just a bad guess.
static const int kNumKnownSuccessesThreshold = 3;

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

HttpPipelinedHostImpl::HttpPipelinedHostImpl(
    HttpPipelinedHost::Delegate* delegate,
    const HostPortPair& origin,
    HttpPipelinedConnection::Factory* factory,
    Capability capability)
    : delegate_(delegate),
      origin_(origin),
      factory_(factory),
      capability_(capability) {
  if (!factory) {
    factory_.reset(new HttpPipelinedConnectionImplFactory());
  }
}

HttpPipelinedHostImpl::~HttpPipelinedHostImpl() {
  CHECK(pipelines_.empty());
}

HttpPipelinedStream* HttpPipelinedHostImpl::CreateStreamOnNewPipeline(
    ClientSocketHandle* connection,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated) {
  if (capability_ == INCAPABLE) {
    return NULL;
  }
  HttpPipelinedConnection* pipeline = factory_->CreateNewPipeline(
      connection, this, used_ssl_config, used_proxy_info, net_log,
      was_npn_negotiated);
  PipelineInfo info;
  pipelines_.insert(std::make_pair(pipeline, info));
  return pipeline->CreateNewStream();
}

HttpPipelinedStream* HttpPipelinedHostImpl::CreateStreamOnExistingPipeline() {
  HttpPipelinedConnection* available_pipeline = NULL;
  for (PipelineInfoMap::iterator it = pipelines_.begin();
       it != pipelines_.end(); ++it) {
    if (it->first->usable() &&
        it->first->active() &&
        it->first->depth() < GetPipelineCapacity() &&
        (!available_pipeline ||
         it->first->depth() < available_pipeline->depth())) {
      available_pipeline = it->first;
    }
  }
  if (!available_pipeline) {
    return NULL;
  }
  return available_pipeline->CreateNewStream();
}

bool HttpPipelinedHostImpl::IsExistingPipelineAvailable() const {
  for (PipelineInfoMap::const_iterator it = pipelines_.begin();
       it != pipelines_.end(); ++it) {
    if (it->first->usable() &&
        it->first->active() &&
        it->first->depth() < GetPipelineCapacity()) {
      return true;
    }
  }
  return false;
}

const HostPortPair& HttpPipelinedHostImpl::origin() const {
  return origin_;
}

void HttpPipelinedHostImpl::OnPipelineEmpty(HttpPipelinedConnection* pipeline) {
  CHECK(ContainsKey(pipelines_, pipeline));
  pipelines_.erase(pipeline);
  delete pipeline;
  if (pipelines_.empty()) {
    delegate_->OnHostIdle(this);
    // WARNING: We'll probably be deleted here.
  }
}

void HttpPipelinedHostImpl::OnPipelineHasCapacity(
    HttpPipelinedConnection* pipeline) {
  CHECK(ContainsKey(pipelines_, pipeline));
  if (pipeline->usable() &&
      capability_ != INCAPABLE &&
      pipeline->depth() < GetPipelineCapacity()) {
    delegate_->OnHostHasAdditionalCapacity(this);
  }
  if (!pipeline->depth()) {
    OnPipelineEmpty(pipeline);
    // WARNING: We might be deleted here.
  }
}

void HttpPipelinedHostImpl::OnPipelineFeedback(
    HttpPipelinedConnection* pipeline,
    HttpPipelinedConnection::Feedback feedback) {
  CHECK(ContainsKey(pipelines_, pipeline));
  switch (feedback) {
    case HttpPipelinedConnection::OK:
      ++pipelines_[pipeline].num_successes;
      if (capability_ == UNKNOWN) {
        capability_ = PROBABLY_CAPABLE;
        for (PipelineInfoMap::iterator it = pipelines_.begin();
             it != pipelines_.end(); ++it) {
          OnPipelineHasCapacity(it->first);
        }
      } else if (capability_ == PROBABLY_CAPABLE &&
                 pipelines_[pipeline].num_successes >=
                     kNumKnownSuccessesThreshold) {
        capability_ = CAPABLE;
        delegate_->OnHostDeterminedCapability(this, CAPABLE);
      }
      break;

    case HttpPipelinedConnection::PIPELINE_SOCKET_ERROR:
    case HttpPipelinedConnection::OLD_HTTP_VERSION:
      capability_ = INCAPABLE;
      delegate_->OnHostDeterminedCapability(this, INCAPABLE);
      break;

    case HttpPipelinedConnection::MUST_CLOSE_CONNECTION:
      break;
  }
}

int HttpPipelinedHostImpl::GetPipelineCapacity() const {
  int capacity = 0;
  switch (capability_) {
    case CAPABLE:
    case PROBABLY_CAPABLE:
      capacity = max_pipeline_depth();
      break;

    case INCAPABLE:
      CHECK(false);

    case UNKNOWN:
      capacity = 1;
      break;

    default:
      CHECK(false) << "Unkown pipeline capability: " << capability_;
  }
  return capacity;
}

HttpPipelinedHostImpl::PipelineInfo::PipelineInfo()
    : num_successes(0) {
}

}  // namespace net
