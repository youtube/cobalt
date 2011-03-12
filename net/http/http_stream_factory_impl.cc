// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl.h"

#include "base/string_number_conversions.h"
#include "base/stl_util-inl.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_factory_impl_job.h"
#include "net/http/http_stream_factory_impl_request.h"
#include "net/spdy/spdy_http_stream.h"

namespace net {

namespace {

bool HasSpdyExclusion(const HostPortPair& endpoint) {
  std::list<HostPortPair>* exclusions =
      HttpStreamFactory::forced_spdy_exclusions();
  if (!exclusions)
    return false;

  std::list<HostPortPair>::const_iterator it;
  for (it = exclusions->begin(); it != exclusions->end(); it++)
    if (it->Equals(endpoint))
      return true;
  return false;
}

GURL UpgradeUrlToHttps(const GURL& original_url) {
  GURL::Replacements replacements;
  // new_sheme and new_port need to be in scope here because GURL::Replacements
  // references the memory contained by them directly.
  const std::string new_scheme = "https";
  const std::string new_port = base::IntToString(443);
  replacements.SetSchemeStr(new_scheme);
  replacements.SetPortStr(new_port);
  return original_url.ReplaceComponents(replacements);
}

}  // namespace

HttpStreamFactoryImpl::HttpStreamFactoryImpl(HttpNetworkSession* session)
    : session_(session) {}

HttpStreamFactoryImpl::~HttpStreamFactoryImpl() {
  DCHECK(request_map_.empty());
  DCHECK(spdy_session_request_map_.empty());

  std::set<const Job*> tmp_job_set;
  tmp_job_set.swap(orphaned_job_set_);
  STLDeleteContainerPointers(tmp_job_set.begin(), tmp_job_set.end());
  DCHECK(orphaned_job_set_.empty());

  tmp_job_set.clear();
  tmp_job_set.swap(preconnect_job_set_);
  STLDeleteContainerPointers(tmp_job_set.begin(), tmp_job_set.end());
  DCHECK(preconnect_job_set_.empty());
}

HttpStreamRequest* HttpStreamFactoryImpl::RequestStream(
    const HttpRequestInfo& request_info,
    const SSLConfig& ssl_config,
    HttpStreamRequest::Delegate* delegate,
    const BoundNetLog& net_log) {
  Request* request = new Request(request_info.url, this, delegate, net_log);

  GURL alternate_url;
  bool has_alternate_protocol =
      GetAlternateProtocolRequestFor(request_info.url, &alternate_url);
  Job* alternate_job = NULL;
  if (has_alternate_protocol) {
    HttpRequestInfo alternate_request_info = request_info;
    alternate_request_info.url = alternate_url;
    alternate_job =
        new Job(this, session_, alternate_request_info, ssl_config, net_log);
    request->AttachJob(alternate_job);
    alternate_job->MarkAsAlternate(request_info.url);
  }

  Job* job = new Job(this, session_, request_info, ssl_config, net_log);
  request->AttachJob(job);
  if (alternate_job) {
    job->WaitFor(alternate_job);
    // Make sure to wait until we call WaitFor(), before starting
    // |alternate_job|, otherwise |alternate_job| will not notify |job|
    // appropriately.
    alternate_job->Start(request);
  }
  // Even if |alternate_job| has already finished, it won't have notified the
  // request yet, since we defer that to the next iteration of the MessageLoop,
  // so starting |job| is always safe.
  job->Start(request);
  return request;
}

void HttpStreamFactoryImpl::PreconnectStreams(
    int num_streams,
    const HttpRequestInfo& request_info,
    const SSLConfig& ssl_config,
    const BoundNetLog& net_log) {
  GURL alternate_url;
  bool has_alternate_protocol =
      GetAlternateProtocolRequestFor(request_info.url, &alternate_url);
  Job* job = NULL;
  if (has_alternate_protocol) {
    HttpRequestInfo alternate_request_info = request_info;
    alternate_request_info.url = alternate_url;
    job = new Job(this, session_, alternate_request_info, ssl_config, net_log);
    job->MarkAsAlternate(request_info.url);
  } else {
    job = new Job(this, session_, request_info, ssl_config, net_log);
  }
  preconnect_job_set_.insert(job);
  job->Preconnect(num_streams);
}

void HttpStreamFactoryImpl::AddTLSIntolerantServer(const HostPortPair& server) {
  tls_intolerant_servers_.insert(server);
}

bool HttpStreamFactoryImpl::IsTLSIntolerantServer(
    const HostPortPair& server) const {
  return ContainsKey(tls_intolerant_servers_, server);
}

bool HttpStreamFactoryImpl::GetAlternateProtocolRequestFor(
    const GURL& original_url,
    GURL* alternate_url) const {
  if (!spdy_enabled())
    return false;

  if (!use_alternate_protocols())
    return false;

  HostPortPair origin = HostPortPair(original_url.HostNoBrackets(),
                                     original_url.EffectiveIntPort());

  const HttpAlternateProtocols& alternate_protocols =
      session_->alternate_protocols();
  if (!alternate_protocols.HasAlternateProtocolFor(origin))
    return false;

  HttpAlternateProtocols::PortProtocolPair alternate =
      alternate_protocols.GetAlternateProtocolFor(origin);
  if (alternate.protocol == HttpAlternateProtocols::BROKEN)
    return false;

  DCHECK_LE(HttpAlternateProtocols::NPN_SPDY_1, alternate.protocol);
  DCHECK_GT(HttpAlternateProtocols::NUM_ALTERNATE_PROTOCOLS,
            alternate.protocol);

  if (alternate.protocol != HttpAlternateProtocols::NPN_SPDY_2)
    return false;

  origin.set_port(alternate.port);
  if (HasSpdyExclusion(origin))
    return false;

  *alternate_url = UpgradeUrlToHttps(original_url);
  return true;
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
    request->Complete(was_npn_negotiated,
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
