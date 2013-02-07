// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl_request.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
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
      protocol_negotiated_(kProtoUnknown),
      using_spdy_(false) {
  DCHECK(factory_);
  DCHECK(delegate_);

  net_log_.BeginEvent(NetLog::TYPE_HTTP_STREAM_REQUEST);
}

HttpStreamFactoryImpl::Request::~Request() {
  if (bound_job_.get())
    DCHECK(jobs_.empty());
  else
    DCHECK(!jobs_.empty());

  net_log_.EndEvent(NetLog::TYPE_HTTP_STREAM_REQUEST);

  for (std::set<Job*>::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    factory_->request_map_.erase(*it);

  RemoveRequestFromSpdySessionRequestMap();
  RemoveRequestFromHttpPipeliningRequestMap();

  STLDeleteElements(&jobs_);
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

bool HttpStreamFactoryImpl::Request::SetHttpPipeliningKey(
    const HttpPipelinedHost::Key& http_pipelining_key) {
  CHECK(!http_pipelining_key_.get());
  http_pipelining_key_.reset(new HttpPipelinedHost::Key(http_pipelining_key));
  bool was_new_key = !ContainsKey(factory_->http_pipelining_request_map_,
                                  http_pipelining_key);
  RequestVector& request_vector =
      factory_->http_pipelining_request_map_[http_pipelining_key];
  request_vector.push_back(this);
  return was_new_key;
}

void HttpStreamFactoryImpl::Request::AttachJob(Job* job) {
  DCHECK(job);
  jobs_.insert(job);
  factory_->request_map_[job] = this;
}

void HttpStreamFactoryImpl::Request::Complete(
    bool was_npn_negotiated,
    NextProto protocol_negotiated,
    bool using_spdy,
    const BoundNetLog& job_net_log) {
  DCHECK(!completed_);
  completed_ = true;
  was_npn_negotiated_ = was_npn_negotiated;
  protocol_negotiated_ = protocol_negotiated;
  using_spdy_ = using_spdy;
  net_log_.AddEvent(
      NetLog::TYPE_HTTP_STREAM_REQUEST_BOUND_TO_JOB,
      job_net_log.source().ToEventParametersCallback());
  job_net_log.AddEvent(
      NetLog::TYPE_HTTP_STREAM_JOB_BOUND_TO_REQUEST,
      net_log_.source().ToEventParametersCallback());
}

void HttpStreamFactoryImpl::Request::OnStreamReady(
    Job* job,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    HttpStreamBase* stream) {
  DCHECK(stream);
  DCHECK(completed_);

  // |job| should only be NULL if we're being serviced by a late bound
  // SpdySession or HttpPipelinedConnection (one that was not created by a job
  // in our |jobs_| set).
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
  // |job| should only be NULL if we're being canceled by a late bound
  // HttpPipelinedConnection (one that was not created by a job in our |jobs_|
  // set).
  if (!job) {
    DCHECK(!bound_job_.get());
    DCHECK(!jobs_.empty());
    // NOTE(willchan): We do *NOT* call OrphanJobs() here. The reason is because
    // we *WANT* to cancel the unnecessary Jobs from other requests if another
    // Job completes first.
  } else if (!bound_job_.get()) {
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
    HttpStreamBase* stream) {
  if (!bound_job_.get())
    OrphanJobsExcept(job);
  else
    DCHECK(jobs_.empty());
  delegate_->OnHttpsProxyTunnelResponse(
      response_info, used_ssl_config, used_proxy_info, stream);
}

int HttpStreamFactoryImpl::Request::RestartTunnelWithProxyAuth(
    const AuthCredentials& credentials) {
  DCHECK(bound_job_.get());
  return bound_job_->RestartTunnelWithProxyAuth(credentials);
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

NextProto HttpStreamFactoryImpl::Request::protocol_negotiated()
    const {
  DCHECK(completed_);
  return protocol_negotiated_;
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

void
HttpStreamFactoryImpl::Request::RemoveRequestFromHttpPipeliningRequestMap() {
  if (http_pipelining_key_.get()) {
    HttpPipeliningRequestMap& http_pipelining_request_map =
        factory_->http_pipelining_request_map_;
    DCHECK(ContainsKey(http_pipelining_request_map, *http_pipelining_key_));
    RequestVector& request_vector =
        http_pipelining_request_map[*http_pipelining_key_];
    for (RequestVector::iterator it = request_vector.begin();
         it != request_vector.end(); ++it) {
      if (*it == this) {
        request_vector.erase(it);
        break;
      }
    }
    if (request_vector.empty())
      http_pipelining_request_map.erase(*http_pipelining_key_);
    http_pipelining_key_.reset();
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
  const SSLConfig used_ssl_config = job->server_ssl_config();
  const ProxyInfo used_proxy_info = job->proxy_info();
  const bool was_npn_negotiated = job->was_npn_negotiated();
  const NextProto protocol_negotiated =
      job->protocol_negotiated();
  const bool using_spdy = job->using_spdy();
  const BoundNetLog net_log = job->net_log();

  Complete(was_npn_negotiated, protocol_negotiated, using_spdy, net_log);

  // Cache this so we can still use it if the request is deleted.
  HttpStreamFactoryImpl* factory = factory_;

  bool use_relative_url = direct || url().SchemeIs("https");
  delegate_->OnStreamReady(
      job->server_ssl_config(),
      job->proxy_info(),
      new SpdyHttpStream(spdy_session, use_relative_url));
  // |this| may be deleted after this point.
  factory->OnSpdySessionReady(
      spdy_session, direct, used_ssl_config, used_proxy_info,
      was_npn_negotiated, protocol_negotiated, using_spdy, net_log);
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
  RemoveRequestFromHttpPipeliningRequestMap();

  std::set<Job*> tmp;
  tmp.swap(jobs_);

  for (std::set<Job*>::iterator it = tmp.begin(); it != tmp.end(); ++it)
    factory_->OrphanJob(*it, this);
}

}  // namespace net
