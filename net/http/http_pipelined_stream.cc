// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_pipelined_stream.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/http/http_pipelined_connection_impl.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_util.h"

namespace net {

HttpPipelinedStream::HttpPipelinedStream(HttpPipelinedConnectionImpl* pipeline,
                                         int pipeline_id)
    : pipeline_(pipeline),
      pipeline_id_(pipeline_id),
      request_info_(NULL) {
}

HttpPipelinedStream::~HttpPipelinedStream() {
  pipeline_->OnStreamDeleted(pipeline_id_);
}

int HttpPipelinedStream::InitializeStream(const HttpRequestInfo* request_info,
                                          const BoundNetLog& net_log,
                                          OldCompletionCallback* callback) {
  request_info_ = request_info;
  pipeline_->InitializeParser(pipeline_id_, request_info, net_log);
  return OK;
}


int HttpPipelinedStream::SendRequest(const HttpRequestHeaders& headers,
                                     UploadDataStream* request_body,
                                     HttpResponseInfo* response,
                                     OldCompletionCallback* callback) {
  CHECK(pipeline_id_);
  CHECK(request_info_);
  // TODO(simonjam): Proxy support will be needed here.
  const std::string path = HttpUtil::PathForRequest(request_info_->url);
  std::string request_line_ = base::StringPrintf("%s %s HTTP/1.1\r\n",
                                                 request_info_->method.c_str(),
                                                 path.c_str());
  return pipeline_->SendRequest(pipeline_id_, request_line_, headers,
                                request_body, response, callback);
}

uint64 HttpPipelinedStream::GetUploadProgress() const {
  return pipeline_->GetUploadProgress(pipeline_id_);
}

int HttpPipelinedStream::ReadResponseHeaders(OldCompletionCallback* callback) {
  return pipeline_->ReadResponseHeaders(pipeline_id_, callback);
}

const HttpResponseInfo* HttpPipelinedStream::GetResponseInfo() const {
  return pipeline_->GetResponseInfo(pipeline_id_);
}

int HttpPipelinedStream::ReadResponseBody(IOBuffer* buf, int buf_len,
                                          OldCompletionCallback* callback) {
  return pipeline_->ReadResponseBody(pipeline_id_, buf, buf_len, callback);
}

void HttpPipelinedStream::Close(bool not_reusable) {
  pipeline_->Close(pipeline_id_, not_reusable);
}

HttpStream* HttpPipelinedStream::RenewStreamForAuth() {
  if (pipeline_->usable()) {
    return pipeline_->CreateNewStream();
  }
  return NULL;
}

bool HttpPipelinedStream::IsResponseBodyComplete() const {
  return pipeline_->IsResponseBodyComplete(pipeline_id_);
}

bool HttpPipelinedStream::CanFindEndOfResponse() const {
  return pipeline_->CanFindEndOfResponse(pipeline_id_);
}

bool HttpPipelinedStream::IsMoreDataBuffered() const {
  return pipeline_->IsMoreDataBuffered(pipeline_id_);
}

bool HttpPipelinedStream::IsConnectionReused() const {
  return pipeline_->IsConnectionReused(pipeline_id_);
}

void HttpPipelinedStream::SetConnectionReused() {
  pipeline_->SetConnectionReused(pipeline_id_);
}

bool HttpPipelinedStream::IsConnectionReusable() const {
  return pipeline_->usable();
}

void HttpPipelinedStream::GetSSLInfo(SSLInfo* ssl_info) {
  pipeline_->GetSSLInfo(pipeline_id_, ssl_info);
}

void HttpPipelinedStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  pipeline_->GetSSLCertRequestInfo(pipeline_id_, cert_request_info);
}

bool HttpPipelinedStream::IsSpdyHttpStream() const {
  return false;
}

void HttpPipelinedStream::LogNumRttVsBytesMetrics() const {
  // TODO(simonjam): I don't want to copy & paste this from http_basic_stream.
}

void HttpPipelinedStream::Drain(HttpNetworkSession* session) {
  pipeline_->Drain(this, session);
}

const SSLConfig& HttpPipelinedStream::used_ssl_config() const {
  return pipeline_->used_ssl_config();
}

const ProxyInfo& HttpPipelinedStream::used_proxy_info() const {
  return pipeline_->used_proxy_info();
}

const NetLog::Source& HttpPipelinedStream::source() const {
  return pipeline_->source();
}

bool HttpPipelinedStream::was_npn_negotiated() const {
  return pipeline_->was_npn_negotiated();
}

}  // namespace net
