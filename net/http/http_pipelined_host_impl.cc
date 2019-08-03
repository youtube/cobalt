// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_impl.h"

#include "base/stl_util.h"
#include "base/values.h"
#include "net/http/http_pipelined_connection_impl.h"
#include "net/http/http_pipelined_stream.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace net {

// TODO(simonjam): Run experiments to see what value minimizes evictions without
// costing too much performance. Until then, this is just a bad guess.
static const int kNumKnownSuccessesThreshold = 3;

HttpPipelinedHostImpl::HttpPipelinedHostImpl(
    HttpPipelinedHost::Delegate* delegate,
    const HttpPipelinedHost::Key& key,
    HttpPipelinedConnection::Factory* factory,
    HttpPipelinedHostCapability capability)
    : delegate_(delegate),
      key_(key),
      factory_(factory),
      capability_(capability) {
  if (!factory) {
    factory_.reset(new HttpPipelinedConnectionImpl::Factory());
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
    bool was_npn_negotiated,
    NextProto protocol_negotiated) {
  if (capability_ == PIPELINE_INCAPABLE) {
    return NULL;
  }
  HttpPipelinedConnection* pipeline = factory_->CreateNewPipeline(
      connection, this, key_.origin(), used_ssl_config, used_proxy_info,
      net_log, was_npn_negotiated, protocol_negotiated);
  PipelineInfo info;
  pipelines_.insert(std::make_pair(pipeline, info));
  return pipeline->CreateNewStream();
}

HttpPipelinedStream* HttpPipelinedHostImpl::CreateStreamOnExistingPipeline() {
  HttpPipelinedConnection* available_pipeline = NULL;
  for (PipelineInfoMap::iterator it = pipelines_.begin();
       it != pipelines_.end(); ++it) {
    if (CanPipelineAcceptRequests(it->first) &&
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
    if (CanPipelineAcceptRequests(it->first)) {
      return true;
    }
  }
  return false;
}

const HttpPipelinedHost::Key& HttpPipelinedHostImpl::GetKey() const {
  return key_;
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
  if (CanPipelineAcceptRequests(pipeline)) {
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
      if (capability_ == PIPELINE_UNKNOWN) {
        capability_ = PIPELINE_PROBABLY_CAPABLE;
        NotifyAllPipelinesHaveCapacity();
      } else if (capability_ == PIPELINE_PROBABLY_CAPABLE &&
                 pipelines_[pipeline].num_successes >=
                     kNumKnownSuccessesThreshold) {
        capability_ = PIPELINE_CAPABLE;
        delegate_->OnHostDeterminedCapability(this, PIPELINE_CAPABLE);
      }
      break;

    case HttpPipelinedConnection::PIPELINE_SOCKET_ERROR:
      // Socket errors on the initial request - when no other requests are
      // pipelined - can't be due to pipelining.
      if (pipelines_[pipeline].num_successes > 0 || pipeline->depth() > 1) {
        // TODO(simonjam): This may be needlessly harsh. For example, pogo.com
        // only returns a socket error once after the root document, but is
        // otherwise able to pipeline just fine. Consider being more persistent
        // and only give up on pipelining if we get a couple of failures.
        capability_ = PIPELINE_INCAPABLE;
        delegate_->OnHostDeterminedCapability(this, PIPELINE_INCAPABLE);
      }
      break;

    case HttpPipelinedConnection::OLD_HTTP_VERSION:
    case HttpPipelinedConnection::AUTHENTICATION_REQUIRED:
      capability_ = PIPELINE_INCAPABLE;
      delegate_->OnHostDeterminedCapability(this, PIPELINE_INCAPABLE);
      break;

    case HttpPipelinedConnection::MUST_CLOSE_CONNECTION:
      break;
  }
}

int HttpPipelinedHostImpl::GetPipelineCapacity() const {
  int capacity = 0;
  switch (capability_) {
    case PIPELINE_CAPABLE:
    case PIPELINE_PROBABLY_CAPABLE:
      capacity = max_pipeline_depth();
      break;

    case PIPELINE_INCAPABLE:
      CHECK(false);

    case PIPELINE_UNKNOWN:
      capacity = 1;
      break;

    default:
      CHECK(false) << "Unkown pipeline capability: " << capability_;
  }
  return capacity;
}

bool HttpPipelinedHostImpl::CanPipelineAcceptRequests(
    HttpPipelinedConnection* pipeline) const {
  return capability_ != PIPELINE_INCAPABLE &&
      pipeline->usable() &&
      pipeline->active() &&
      pipeline->depth() < GetPipelineCapacity();
}

void HttpPipelinedHostImpl::NotifyAllPipelinesHaveCapacity() {
  // Calling OnPipelineHasCapacity() can have side effects that include
  // deleting and removing entries from |pipelines_|.
  PipelineInfoMap pipelines_to_notify = pipelines_;
  for (PipelineInfoMap::iterator it = pipelines_to_notify.begin();
       it != pipelines_to_notify.end(); ++it) {
    if (pipelines_.find(it->first) != pipelines_.end()) {
      OnPipelineHasCapacity(it->first);
    }
  }
}

Value* HttpPipelinedHostImpl::PipelineInfoToValue() const {
  ListValue* list_value = new ListValue();
  for (PipelineInfoMap::const_iterator it = pipelines_.begin();
       it != pipelines_.end(); ++it) {
    DictionaryValue* pipeline_dict = new DictionaryValue;
    pipeline_dict->SetString("host", key_.origin().ToString());
    pipeline_dict->SetBoolean("forced", false);
    pipeline_dict->SetInteger("depth", it->first->depth());
    pipeline_dict->SetInteger("capacity", GetPipelineCapacity());
    pipeline_dict->SetBoolean("usable", it->first->usable());
    pipeline_dict->SetBoolean("active", it->first->active());
    pipeline_dict->SetInteger("source_id", it->first->net_log().source().id);
    list_value->Append(pipeline_dict);
  }
  return list_value;
}

HttpPipelinedHostImpl::PipelineInfo::PipelineInfo()
    : num_successes(0) {
}

}  // namespace net
