// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_host_forced.h"

#include "base/values.h"
#include "net/http/http_pipelined_connection_impl.h"
#include "net/http/http_pipelined_stream.h"
#include "net/socket/buffered_write_stream_socket.h"
#include "net/socket/client_socket_handle.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace net {

HttpPipelinedHostForced::HttpPipelinedHostForced(
    HttpPipelinedHost::Delegate* delegate,
    const Key& key,
    HttpPipelinedConnection::Factory* factory)
    : delegate_(delegate),
      key_(key),
      factory_(factory) {
  if (!factory) {
    factory_.reset(new HttpPipelinedConnectionImpl::Factory());
  }
}

HttpPipelinedHostForced::~HttpPipelinedHostForced() {
  CHECK(!pipeline_.get());
}

HttpPipelinedStream* HttpPipelinedHostForced::CreateStreamOnNewPipeline(
    ClientSocketHandle* connection,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    const BoundNetLog& net_log,
    bool was_npn_negotiated,
    NextProto protocol_negotiated) {
  CHECK(!pipeline_.get());
  StreamSocket* wrapped_socket = connection->release_socket();
  BufferedWriteStreamSocket* buffered_socket = new BufferedWriteStreamSocket(
      wrapped_socket);
  connection->set_socket(buffered_socket);
  pipeline_.reset(factory_->CreateNewPipeline(
      connection, this, key_.origin(), used_ssl_config, used_proxy_info,
      net_log, was_npn_negotiated, protocol_negotiated));
  return pipeline_->CreateNewStream();
}

HttpPipelinedStream* HttpPipelinedHostForced::CreateStreamOnExistingPipeline() {
  if (!pipeline_.get()) {
    return NULL;
  }
  return pipeline_->CreateNewStream();
}

bool HttpPipelinedHostForced::IsExistingPipelineAvailable() const {
  return pipeline_.get() != NULL;
}

const HttpPipelinedHost::Key& HttpPipelinedHostForced::GetKey() const {
  return key_;
}

void HttpPipelinedHostForced::OnPipelineEmpty(
    HttpPipelinedConnection* pipeline) {
  CHECK_EQ(pipeline_.get(), pipeline);
  pipeline_.reset();
  delegate_->OnHostIdle(this);
  // WARNING: We'll probably be deleted here.
}

void HttpPipelinedHostForced::OnPipelineHasCapacity(
    HttpPipelinedConnection* pipeline) {
  CHECK_EQ(pipeline_.get(), pipeline);
  delegate_->OnHostHasAdditionalCapacity(this);
  if (!pipeline->depth()) {
    OnPipelineEmpty(pipeline);
    // WARNING: We might be deleted here.
  }
}

void HttpPipelinedHostForced::OnPipelineFeedback(
    HttpPipelinedConnection* pipeline,
    HttpPipelinedConnection::Feedback feedback) {
  // We don't care. We always pipeline.
}

Value* HttpPipelinedHostForced::PipelineInfoToValue() const {
  ListValue* list_value = new ListValue();
  if (pipeline_.get()) {
    DictionaryValue* pipeline_dict = new DictionaryValue;
    pipeline_dict->SetString("host", key_.origin().ToString());
    pipeline_dict->SetBoolean("forced", true);
    pipeline_dict->SetInteger("depth", pipeline_->depth());
    pipeline_dict->SetInteger("capacity", 1000);
    pipeline_dict->SetBoolean("usable", pipeline_->usable());
    pipeline_dict->SetBoolean("active", pipeline_->active());
    pipeline_dict->SetInteger("source_id", pipeline_->net_log().source().id);
    list_value->Append(pipeline_dict);
  }
  return list_value;
}

}  // namespace net
