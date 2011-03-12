// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_request.h"

#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "net/http/http_stream_factory_impl_job.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_session.h"

namespace net {

HttpStreamFactoryImpl::Request::Request(const GURL& url,
                                        HttpStreamFactoryImpl* factory,
                                        HttpStreamRequest::Delegate* delegate,
                                        const BoundNetLog& net_log)
    : url_(url),
      factory_(factory),
      delegate_(delegate),
      net_log_(net_log),
      completed_(false),
      was_npn_negotiated_(false),
      using_spdy_(false) {
  DCHECK(factory_);
  DCHECK(delegate_);

  net_log_.BeginEvent(NetLog::TYPE_HTTP_STREAM_REQUEST, NULL);
}

HttpStreamFactoryImpl::Request::~Request() {
  if (bound_job_.get())
    DCHECK(jobs_.empty());
  else
    DCHECK(!jobs_.empty());

  net_log_.EndEvent(NetLog::TYPE_HTTP_STREAM_REQUEST, NULL);

  for (std::set<Job*>::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    factory_->request_map_.erase(*it);

  STLDeleteElements(&jobs_);

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

void HttpStreamFactoryImpl::Request::AttachJob(Job* job) {
  DCHECK(job);
  jobs_.insert(job);
  factory_->request_map_[job] = this;
}

void HttpStreamFactoryImpl::Request::Complete(
    bool was_npn_negotiated,
    bool using_spdy,
    const NetLog::Source& job_source) {
  DCHECK(!completed_);
  completed_ = true;
  was_npn_negotiated_ = was_npn_negotiated;
  using_spdy_ = using_spdy;
  net_log_.AddEvent(
      NetLog::TYPE_HTTP_STREAM_REQUEST_BOUND_TO_JOB,
      make_scoped_refptr(new NetLogSourceParameter(
          "source_dependency", job_source)));
}

void HttpStreamFactoryImpl::Request::OnStreamReady(
    Job* job,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpStream* stream) {
  DCHECK(stream);
  DCHECK(completed_);

  // |job| should only be NULL if we're being serviced by a late bound
  // SpdySession (one that was not created by a job in our |jobs_| set).
  if (!job) {
    DCHECK(!bound_job_.get());
    DCHECK(!jobs_.empty());
    // NOTE(willchan): We do *NOT* call OrphanJobs() here. The reason is because
    // we *WANT* to cancel the unnecessary Jobs from other requests if another
    // Job completes first.
    // TODO(mbelshe): Revisit this when we implement ip connection pooling of
    // SpdySessions. Do we want to orphan the jobs for a different hostname so
    // they complete? Or do we want to prevent connecting a new SpdySession if
    // we've already got one available for a different hostname where the ip
    // address matches up?
  } else if (!bound_job_.get()) {
    // We may have other jobs in |jobs_|. For example, if we start multiple jobs
    // for Alternate-Protocol.
    OrphanJobsExcept(job);
  } else {
    DCHECK(jobs_.empty());
  }
  delegate_->OnStreamReady(used_ssl_config, used_proxy_info, stream);
}

void HttpStreamFactoryImpl::Request::OnStreamFailed(
    Job* job,
    int status,
    const SSLConfig& used_ssl_config) {
  DCHECK_NE(OK, status);
  if (!bound_job_.get()) {
    // Hey, we've got other jobs! Maybe one of them will succeed, let's just
    // ignore this failure.
    if (jobs_.size() > 1) {
      jobs_.erase(job);
      factory_->request_map_.erase(job);
      delete job;
      return;
    } else {
      bound_job_.reset(job);
      jobs_.erase(job);
      DCHECK(jobs_.empty());
      factory_->request_map_.erase(job);
    }
  } else {
    DCHECK(jobs_.empty());
  }
  delegate_->OnStreamFailed(status, used_ssl_config);
}

void HttpStreamFactoryImpl::Request::OnCertificateError(
    Job* job,
    int status,
    const SSLConfig& used_ssl_config,
    const SSLInfo& ssl_info) {
  DCHECK_NE(OK, status);
  if (!bound_job_.get())
    OrphanJobsExcept(job);
  else
    DCHECK(jobs_.empty());
  delegate_->OnCertificateError(status, used_ssl_config, ssl_info);
}

void HttpStreamFactoryImpl::Request::OnNeedsProxyAuth(
    Job* job,
    const HttpResponseInfo& proxy_response,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpAuthController* auth_controller) {
  if (!bound_job_.get())
    OrphanJobsExcept(job);
  else
    DCHECK(jobs_.empty());
  delegate_->OnNeedsProxyAuth(
      proxy_response, used_ssl_config, used_proxy_info, auth_controller);
}

void HttpStreamFactoryImpl::Request::OnNeedsClientAuth(
    Job* job,
    const SSLConfig& used_ssl_config,
    SSLCertRequestInfo* cert_info) {
  if (!bound_job_.get())
    OrphanJobsExcept(job);
  else
    DCHECK(jobs_.empty());
  delegate_->OnNeedsClientAuth(used_ssl_config, cert_info);
}

void HttpStreamFactoryImpl::Request::OnHttpsProxyTunnelResponse(
    Job *job,
    const HttpResponseInfo& response_info,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpStream* stream) {
  if (!bound_job_.get())
    OrphanJobsExcept(job);
  else
    DCHECK(jobs_.empty());
  delegate_->OnHttpsProxyTunnelResponse(
      response_info, used_ssl_config, used_proxy_info, stream);
}

int HttpStreamFactoryImpl::Request::RestartTunnelWithProxyAuth(
    const string16& username,
    const string16& password) {
  DCHECK(bound_job_.get());
  return bound_job_->RestartTunnelWithProxyAuth(username, password);
}

LoadState HttpStreamFactoryImpl::Request::GetLoadState() const {
  if (bound_job_.get())
    return bound_job_->GetLoadState();
  DCHECK(!jobs_.empty());

  // Just pick the first one.
  return (*jobs_.begin())->GetLoadState();
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

void HttpStreamFactoryImpl::Request::OnSpdySessionReady(
    Job* job,
    scoped_refptr<SpdySession> spdy_session,
    bool direct) {
  DCHECK(job);
  DCHECK(job->using_spdy());

  // The first case is the usual case.
  if (!bound_job_.get()) {
    OrphanJobsExcept(job);
  } else { // This is the case for HTTPS proxy tunneling.
    DCHECK_EQ(bound_job_.get(), job);
    DCHECK(jobs_.empty());
  }

  // Cache these values in case the job gets deleted.
  const SSLConfig used_ssl_config = job->ssl_config();
  const ProxyInfo used_proxy_info = job->proxy_info();
  const bool was_npn_negotiated = job->was_npn_negotiated();
  const bool using_spdy = job->using_spdy();
  const NetLog::Source source = job->net_log().source();

  Complete(was_npn_negotiated,
           using_spdy,
           source);

  // Cache this so we can still use it if the request is deleted.
  HttpStreamFactoryImpl* factory = factory_;

  bool use_relative_url = direct || url().SchemeIs("https");
  delegate_->OnStreamReady(
      job->ssl_config(),
      job->proxy_info(),
      new SpdyHttpStream(spdy_session, use_relative_url));
  // |this| may be deleted after this point.
  factory->OnSpdySessionReady(
      spdy_session, direct, used_ssl_config, used_proxy_info,
      was_npn_negotiated, using_spdy, source);
}

void HttpStreamFactoryImpl::Request::OrphanJobsExcept(Job* job) {
  DCHECK(job);
  DCHECK(!bound_job_.get());
  DCHECK(ContainsKey(jobs_, job));
  bound_job_.reset(job);
  jobs_.erase(job);
  factory_->request_map_.erase(job);

  OrphanJobs();
}

void HttpStreamFactoryImpl::Request::OrphanJobs() {
  RemoveRequestFromSpdySessionRequestMap();

  std::set<Job*> tmp;
  tmp.swap(jobs_);

  for (std::set<Job*>::iterator it = tmp.begin(); it != tmp.end(); ++it)
    factory_->OrphanJob(*it, this);
}

}  // namespace net
