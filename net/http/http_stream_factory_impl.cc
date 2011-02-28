// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl.h"

#include "base/stl_util-inl.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_factory_impl_job.h"
#include "net/http/http_stream_factory_impl_request.h"
#include "net/spdy/spdy_http_stream.h"

namespace net {

HttpStreamFactoryImpl::HttpStreamFactoryImpl(HttpNetworkSession* session)
    : session_(session) {}

HttpStreamFactoryImpl::~HttpStreamFactoryImpl() {
  DCHECK(request_map_.empty());
  DCHECK(spdy_session_request_map_.empty());

  std::set<const Job*> tmp_job_set;
  tmp_job_set.swap(preconnect_job_set_);
  STLDeleteContainerPointers(tmp_job_set.begin(), tmp_job_set.end());
  DCHECK(preconnect_job_set_.empty());
}

HttpStreamRequest* HttpStreamFactoryImpl::RequestStream(
    const HttpRequestInfo& request_info,
    const SSLConfig& ssl_config,
    HttpStreamRequest::Delegate* delegate,
    const BoundNetLog& net_log) {
  Job* job = new Job(this, session_);
  Request* request = new Request(request_info.url, this, delegate, net_log);
  request_map_[job] = request;
  request->BindJob(job);
  job->Start(request, request_info, ssl_config, net_log);
  return request;
}

void HttpStreamFactoryImpl::PreconnectStreams(
    int num_streams,
    const HttpRequestInfo& request_info,
    const SSLConfig& ssl_config,
    const BoundNetLog& net_log) {
  Job* job = new Job(this, session_);
  preconnect_job_set_.insert(job);
  job->Preconnect(num_streams, request_info, ssl_config, net_log);
}

void HttpStreamFactoryImpl::AddTLSIntolerantServer(const GURL& url) {
  tls_intolerant_servers_.insert(GetHostAndPort(url));
}

bool HttpStreamFactoryImpl::IsTLSIntolerantServer(const GURL& url) const {
  return ContainsKey(tls_intolerant_servers_, GetHostAndPort(url));
}

LoadState HttpStreamFactoryImpl::GetLoadState(const Request& request) const {
  // TODO(willchan): Will break when we do late binding.
  return request.job()->GetLoadState();
}

void HttpStreamFactoryImpl::OnSpdySessionReady(
    const Job* job,
    scoped_refptr<SpdySession> spdy_session,
    bool direct) {
  DCHECK(job->using_spdy());
  DCHECK(ContainsKey(request_map_, job));
  Request* request = request_map_[job];
  request->Complete(job->was_alternate_protocol_available(),
                    job->was_npn_negotiated(),
                    job->using_spdy(),
                    job->net_log().source());
  bool use_relative_url = direct || request->url().SchemeIs("https");
  request->OnStreamReady(
      job->ssl_config(),
      job->proxy_info(),
      new SpdyHttpStream(spdy_session, use_relative_url));
}

void HttpStreamFactoryImpl::OnPreconnectsComplete(const Job* job) {
  preconnect_job_set_.erase(job);
  delete job;
  OnPreconnectsCompleteInternal();
}

}  // namespace net
