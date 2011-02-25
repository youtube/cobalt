// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_request.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "net/http/http_stream_factory_impl_job.h"

namespace net {

HttpStreamFactoryImpl::Request::Request(const GURL& url,
                                        HttpStreamFactoryImpl* factory,
                                        HttpStreamRequest::Delegate* delegate)
    : url_(url),
      factory_(factory),
      delegate_(delegate),
      job_(NULL),
      completed_(false),
      was_alternate_protocol_available_(false),
      was_npn_negotiated_(false),
      using_spdy_(false) {
  DCHECK(factory_);
  DCHECK(delegate_);
}

HttpStreamFactoryImpl::Request::~Request() {
  factory_->request_map_.erase(job_);

  // TODO(willchan): Remove this when we decouple requests and jobs.
  delete job_;

  RemoveRequestFromSpdySessionRequestMap();
}

void HttpStreamFactoryImpl::Request::SetSpdySessionKey(
    const HostPortProxyPair& spdy_session_key) {
  DCHECK(!spdy_session_key_.get());
  spdy_session_key_.reset(new HostPortProxyPair(spdy_session_key));
  RequestSet& request_set =
      factory_->spdy_session_request_map_[spdy_session_key];
  DCHECK(!ContainsKey(request_set, this));
  request_set.insert(this);
}

void HttpStreamFactoryImpl::Request::BindJob(HttpStreamFactoryImpl::Job* job) {
  DCHECK(job);
  DCHECK(!job_);
  job_ = job;
}

void HttpStreamFactoryImpl::Request::Complete(
    bool was_alternate_protocol_available,
    bool was_npn_negotiated,
    bool using_spdy) {
  DCHECK(!completed_);
  completed_ = true;
  was_alternate_protocol_available_ = was_alternate_protocol_available;
  was_npn_negotiated_ = was_npn_negotiated;
  using_spdy_ = using_spdy;
}

void HttpStreamFactoryImpl::Request::OnStreamReady(
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpStream* stream) {
  DCHECK(stream);
  DCHECK(completed_);
  delegate_->OnStreamReady(used_ssl_config, used_proxy_info, stream);
}

void HttpStreamFactoryImpl::Request::OnStreamFailed(
    int status,
    const SSLConfig& used_ssl_config) {
  DCHECK_NE(OK, status);
  delegate_->OnStreamFailed(status, used_ssl_config);
}

void HttpStreamFactoryImpl::Request::OnCertificateError(
    int status,
    const SSLConfig& used_ssl_config,
    const SSLInfo& ssl_info) {
  DCHECK_NE(OK, status);
  delegate_->OnCertificateError(status, used_ssl_config, ssl_info);
}

void HttpStreamFactoryImpl::Request::OnNeedsProxyAuth(
    const HttpResponseInfo& proxy_response,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  delegate_->OnNeedsProxyAuth(
      proxy_response, used_ssl_config, used_proxy_info, auth_controller);
}

void HttpStreamFactoryImpl::Request::OnNeedsClientAuth(
    const SSLConfig& used_ssl_config,
    SSLCertRequestInfo* cert_info) {
  delegate_->OnNeedsClientAuth(used_ssl_config, cert_info);
}

void HttpStreamFactoryImpl::Request::OnHttpsProxyTunnelResponse(
    const HttpResponseInfo& response_info,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpStream* stream) {
  delegate_->OnHttpsProxyTunnelResponse(
      response_info, used_ssl_config, used_proxy_info, stream);
}

int HttpStreamFactoryImpl::Request::RestartTunnelWithProxyAuth(
    const string16& username,
    const string16& password) {
  // We're restarting the job, so ditch the old key. Note that we could actually
  // keep it around and eliminate the DCHECK in set_spdy_session_key() that
  // |spdy_session_key_| is NULL, but I prefer to keep the assertion.
  RemoveRequestFromSpdySessionRequestMap();
  return job_->RestartTunnelWithProxyAuth(username, password);
}

LoadState HttpStreamFactoryImpl::Request::GetLoadState() const {
  return factory_->GetLoadState(*this);
}

bool HttpStreamFactoryImpl::Request::was_alternate_protocol_available() const {
  DCHECK(completed_);
  return was_alternate_protocol_available_;
}

bool HttpStreamFactoryImpl::Request::was_npn_negotiated() const {
  DCHECK(completed_);
  return was_npn_negotiated_;
}

bool HttpStreamFactoryImpl::Request::using_spdy() const {
  DCHECK(completed_);
  return using_spdy_;
}

void
HttpStreamFactoryImpl::Request::RemoveRequestFromSpdySessionRequestMap() {
  if (spdy_session_key_.get()) {
    SpdySessionRequestMap& spdy_session_request_map =
        factory_->spdy_session_request_map_;
    DCHECK(ContainsKey(spdy_session_request_map, *spdy_session_key_));
    RequestSet& request_set =
        spdy_session_request_map[*spdy_session_key_];
    DCHECK(ContainsKey(request_set, this));
    request_set.erase(this);
    if (request_set.empty())
      spdy_session_request_map.erase(*spdy_session_key_);
    spdy_session_key_.reset();
  }
}

}  // namespace net
