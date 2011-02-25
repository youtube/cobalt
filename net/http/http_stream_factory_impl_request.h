// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_IMPL_REQUEST_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_IMPL_REQUEST_H_

#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_stream_factory_impl.h"

namespace net {

class HttpStreamFactoryImpl::Request : public HttpStreamRequest {
 public:
  Request(const GURL& url,
          HttpStreamFactoryImpl* factory,
          HttpStreamRequest::Delegate* delegate);
  virtual ~Request();

  // Returns the Job that the Request started up.
  Job* job() const { return job_; }

  // The GURL from the HttpRequestInfo the started the Request.
  const GURL& url() const { return url_; }

  // Called when the Job determines the appropriate |spdy_session_key| for the
  // Request. Note that this does not mean that SPDY is necessarily supported
  // for this HostPortProxyPair, since we may need to wait for NPN to complete
  // before knowing if SPDY is available.
  void SetSpdySessionKey(const HostPortProxyPair& spdy_session_key);

  // Binds |job| to this request.
  void BindJob(HttpStreamFactoryImpl::Job* job);

  // Marks completion of the request. Must be called before OnStreamReady().
  void Complete(bool was_alternate_protocol_available,
                bool was_npn_negotiated,
                bool using_spdy);

  // If this Request has a spdy_session_key, remove this session from the
  // SpdySessionRequestMap.
  void RemoveRequestFromSpdySessionRequestMap();

  // HttpStreamRequest::Delegate methods which we implement. Note we don't
  // actually subclass HttpStreamRequest::Delegate.

  void OnStreamReady(const SSLConfig& used_ssl_config,
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
  const GURL url_;
  HttpStreamFactoryImpl* const factory_;
  HttpStreamRequest::Delegate* const delegate_;

  // The |job_| that this request is tied to.
  HttpStreamFactoryImpl::Job* job_;
  scoped_ptr<const HostPortProxyPair> spdy_session_key_;

  bool completed_;
  bool was_alternate_protocol_available_;
  bool was_npn_negotiated_;
  bool using_spdy_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

}  // namespace net
#endif  // NET_HTTP_HTTP_STREAM_FACTORY_IMPL_H_
