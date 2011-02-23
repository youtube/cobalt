// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory_impl.h"

#include "base/stl_util-inl.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_factory_impl_job.h"

namespace net {

class HttpStreamFactoryImpl::Request : public HttpStreamRequest {
 public:
  Request(HttpStreamFactoryImpl* factory,
          HttpStreamRequest::Delegate* delegate);
  virtual ~Request();

  Job* job() const { return job_; }

  // Binds |job| to this request.
  void BindJob(HttpStreamFactoryImpl::Job* job);

  // Marks completion of the request. Must be called before OnStreamReady().
  void Complete(bool was_alternate_protocol_available,
                bool was_npn_negotiated,
                bool using_spdy);

  // HttpStreamRequest::Delegate methods which we implement. Note we don't
  // actually subclass HttpStreamRequest::Delegate.

  void OnStreamReady(
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      HttpStream* stream);
  void OnStreamFailed(int status, const SSLConfig& used_ssl_config);
  void OnCertificateError(int status,
                          const SSLConfig& used_ssl_config,
                          const SSLInfo& ssl_info);
  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const SSLConfig& used_ssl_config,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller);
  void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                         SSLCertRequestInfo* cert_info);
  void OnHttpsProxyTunnelResponse(
      const HttpResponseInfo& response_info,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      HttpStream* stream);

  // HttpStreamRequest methods.

  virtual int RestartTunnelWithProxyAuth(const string16& username,
                                         const string16& password);
  virtual LoadState GetLoadState() const;
  virtual bool was_alternate_protocol_available() const;
  virtual bool was_npn_negotiated() const;
  virtual bool using_spdy() const;

 private:
  HttpStreamFactoryImpl* const factory_;
  HttpStreamRequest::Delegate* const delegate_;

  // The |job_| that this request is tied to.
  // TODO(willchan): Revisit this when we decouple requests and jobs further.
  HttpStreamFactoryImpl::Job* job_;

  bool completed_;
  bool was_alternate_protocol_available_;
  bool was_npn_negotiated_;
  bool using_spdy_;
  DISALLOW_COPY_AND_ASSIGN(Request);
};

HttpStreamFactoryImpl::Request::Request(HttpStreamFactoryImpl* factory,
                                        HttpStreamRequest::Delegate* delegate)
    : factory_(factory),
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

HttpStreamFactoryImpl::HttpStreamFactoryImpl(HttpNetworkSession* session)
    : session_(session) {}

HttpStreamFactoryImpl::~HttpStreamFactoryImpl() {
  DCHECK(request_map_.empty());

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
  Request* request = new Request(this, delegate);
  request_map_[job] = request;
  request->BindJob(job);
  job->Start(request_info, ssl_config, net_log);
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

void HttpStreamFactoryImpl::OnStreamReady(const Job* job,
                                          const SSLConfig& ssl_config,
                                          const ProxyInfo& proxy_info,
                                          HttpStream* stream) {
  DCHECK(ContainsKey(request_map_, job));
  Request* request = request_map_[job];
  request->Complete(job->was_alternate_protocol_available(),
                    job->was_npn_negotiated(),
                    job->using_spdy());
  request->OnStreamReady(ssl_config, proxy_info, stream);
}

void HttpStreamFactoryImpl::OnStreamFailed(const Job* job,
                                           int result,
                                           const SSLConfig& ssl_config) {
  DCHECK(ContainsKey(request_map_, job));
  request_map_[job]->OnStreamFailed(result, ssl_config);
}

void HttpStreamFactoryImpl::OnCertificateError(const Job* job,
                                               int result,
                                               const SSLConfig& ssl_config,
                                               const SSLInfo& ssl_info) {
  DCHECK(ContainsKey(request_map_, job));
  request_map_[job]->OnCertificateError(result, ssl_config, ssl_info);
}

void HttpStreamFactoryImpl::OnNeedsProxyAuth(
    const Job* job,
    const HttpResponseInfo& response,
    const SSLConfig& ssl_config,
    const ProxyInfo& proxy_info,
    HttpAuthController* auth_controller) {
  DCHECK(ContainsKey(request_map_, job));
  request_map_[job]->OnNeedsProxyAuth(
      response, ssl_config, proxy_info, auth_controller);
}

void HttpStreamFactoryImpl::OnNeedsClientAuth(
    const Job* job,
    const SSLConfig& ssl_config,
    SSLCertRequestInfo* cert_info) {
  DCHECK(ContainsKey(request_map_, job));
  request_map_[job]->OnNeedsClientAuth(ssl_config, cert_info);
}

void HttpStreamFactoryImpl::OnHttpsProxyTunnelResponse(
    const Job* job,
    const HttpResponseInfo& response_info,
    const SSLConfig& ssl_config,
    const ProxyInfo& proxy_info,
    HttpStream* stream) {
  DCHECK(ContainsKey(request_map_, job));
  request_map_[job]->OnHttpsProxyTunnelResponse(
      response_info, ssl_config, proxy_info, stream);
}

void HttpStreamFactoryImpl::OnPreconnectsComplete(const Job* job) {
  preconnect_job_set_.erase(job);
  delete job;
  OnPreconnectsCompleteInternal();
}

}  // namespace net
