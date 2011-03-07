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
  request->AttachJob(job);
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

void HttpStreamFactoryImpl::OrphanJob(Job* job, const Request* request) {
  DCHECK(ContainsKey(request_map_, job));
  DCHECK_EQ(request_map_[job], request);
  DCHECK(!ContainsKey(orphaned_job_set_, job));

  request_map_.erase(job);

  orphaned_job_set_.insert(job);
  job->Orphan(request);
}

void HttpStreamFactoryImpl::OnSpdySessionReady(
    scoped_refptr<SpdySession> spdy_session,
    bool direct,
    const SSLConfig& used_ssl_config,
    const ProxyInfo& used_proxy_info,
    bool was_alternate_protocol_available,
    bool was_npn_negotiated,
    bool using_spdy,
    const NetLog::Source& source) {
  const HostPortProxyPair& spdy_session_key =
      spdy_session->host_port_proxy_pair();
  while (!spdy_session->IsClosed()) {
    // Each iteration may empty out the RequestSet for |spdy_session_key_ in
    // |spdy_session_request_map_|. So each time, check for RequestSet and use
    // the first one.
    //
    // TODO(willchan): If it's important, switch RequestSet out for a FIFO
    // pqueue (Order by priority first, then FIFO within same priority). Unclear
    // that it matters here.
    if (!ContainsKey(spdy_session_request_map_, spdy_session_key))
      break;
    Request* request = *spdy_session_request_map_[spdy_session_key].begin();
    request->Complete(was_alternate_protocol_available,
                      was_npn_negotiated,
                      using_spdy,
                      source);
    bool use_relative_url = direct || request->url().SchemeIs("https");
    request->OnStreamReady(NULL, used_ssl_config, used_proxy_info,
                           new SpdyHttpStream(spdy_session, use_relative_url));
  }
  // TODO(mbelshe): Alert other valid requests.
}

void HttpStreamFactoryImpl::OnOrphanedJobComplete(const Job* job) {
  orphaned_job_set_.erase(job);
  delete job;
}

void HttpStreamFactoryImpl::OnPreconnectsComplete(const Job* job) {
  preconnect_job_set_.erase(job);
  delete job;
  OnPreconnectsCompleteInternal();
}

}  // namespace net
