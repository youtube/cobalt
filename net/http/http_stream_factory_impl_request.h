// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_IMPL_REQUEST_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_IMPL_REQUEST_H_

#include <set>
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_log.h"
#include "net/http/http_stream_factory_impl.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class HttpStreamFactoryImpl::Request : public HttpStreamRequest {
 public:
  Request(const GURL& url,
          HttpStreamFactoryImpl* factory,
          HttpStreamRequest::Delegate* delegate,
          const BoundNetLog& net_log);
  virtual ~Request();

  // The GURL from the HttpRequestInfo the started the Request.
  const GURL& url() const { return url_; }

  // Called when the Job determines the appropriate |spdy_session_key| for the
  // Request. Note that this does not mean that SPDY is necessarily supported
  // for this HostPortProxyPair, since we may need to wait for NPN to complete
  // before knowing if SPDY is available.
  void SetSpdySessionKey(const HostPortProxyPair& spdy_session_key);

  // Called when the Job determines the appropriate |http_pipelining_key| for
  // the Request. Registers this Request with the factory, so that if an
  // existing pipeline becomes available, this Request can be late bound to it.
  // Returns true if this is this key was new to the factory.
  bool SetHttpPipeliningKey(const HttpPipelinedHost::Key& http_pipelining_key);
  bool HasSpdySessionKey() const;

  // Attaches |job| to this request. Does not mean that Request will use |job|,
  // but Request will own |job|.
  void AttachJob(HttpStreamFactoryImpl::Job* job);

  // Marks completion of the request. Must be called before OnStreamReady().
  // |job_net_log| is the BoundNetLog of the Job that fulfilled this request.
  void Complete(bool was_npn_negotiated,
                NextProto protocol_negotiated,
                bool using_spdy,
                const BoundNetLog& job_net_log);

  // If this Request has a |spdy_session_key_|, remove this session from the
  // SpdySessionRequestMap.
  void RemoveRequestFromSpdySessionRequestMap();

  // If this Request has a |http_pipelining_key_|, remove this session from the
  // HttpPipeliningRequestMap.
  void RemoveRequestFromHttpPipeliningRequestMap();

  // Called by an attached Job if it sets up a SpdySession.
  void OnSpdySessionReady(Job* job,
                          scoped_refptr<SpdySession> spdy_session,
                          bool direct);

  // HttpStreamRequest::Delegate methods which we implement. Note we don't
  // actually subclass HttpStreamRequest::Delegate.

  void OnStreamReady(Job* job,
                     const SSLConfig& used_ssl_config,
                     const ProxyInfo& used_proxy_info,
                     HttpStreamBase* stream);
  void OnStreamFailed(Job* job, int status, const SSLConfig& used_ssl_config);
  void OnCertificateError(Job* job,
                          int status,
                          const SSLConfig& used_ssl_config,
                          const SSLInfo& ssl_info);
  void OnNeedsProxyAuth(Job* job,
                        const HttpResponseInfo& proxy_response,
                        const SSLConfig& used_ssl_config,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller);
  void OnNeedsClientAuth(Job* job,
                         const SSLConfig& used_ssl_config,
                         SSLCertRequestInfo* cert_info);
  void OnHttpsProxyTunnelResponse(
      Job *job,
      const HttpResponseInfo& response_info,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      HttpStreamBase* stream);

  // HttpStreamRequest methods.

  virtual int RestartTunnelWithProxyAuth(
      const AuthCredentials& credentials) override;
  virtual LoadState GetLoadState() const override;
  virtual bool was_npn_negotiated() const override;
  virtual NextProto protocol_negotiated() const override;
  virtual bool using_spdy() const override;

 private:
  // Used to orphan all jobs in |jobs_| other than |job| which becomes "bound"
  // to the request.
  void OrphanJobsExcept(Job* job);

  // Used to orphan all jobs in |jobs_|.
  void OrphanJobs();

  const GURL url_;
  HttpStreamFactoryImpl* const factory_;
  HttpStreamRequest::Delegate* const delegate_;
  const BoundNetLog net_log_;

  // At the point where Job is irrevocably tied to the Request, we set this.
  scoped_ptr<Job> bound_job_;
  std::set<HttpStreamFactoryImpl::Job*> jobs_;
  scoped_ptr<const HostPortProxyPair> spdy_session_key_;
  scoped_ptr<const HttpPipelinedHost::Key> http_pipelining_key_;

  bool completed_;
  bool was_npn_negotiated_;
  // Protocol negotiated with the server.
  NextProto protocol_negotiated_;
  bool using_spdy_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_IMPL_H_
