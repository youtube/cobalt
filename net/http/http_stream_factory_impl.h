// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_IMPL_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_IMPL_H_

#include <map>
#include <set>
#include <string>

#include "net/http/http_stream_factory.h"

namespace net {

class HttpNetworkSession;

class HttpStreamFactoryImpl : public HttpStreamFactory {
 public:
  explicit HttpStreamFactoryImpl(HttpNetworkSession* session);
  virtual ~HttpStreamFactoryImpl();

  // HttpStreamFactory Interface
  virtual HttpStreamRequest* RequestStream(
      const HttpRequestInfo& info,
      const SSLConfig& ssl_config,
      HttpStreamRequest::Delegate* delegate,
      const BoundNetLog& net_log);

  virtual void PreconnectStreams(int num_streams,
                                 const HttpRequestInfo& info,
                                 const SSLConfig& ssl_config,
                                 const BoundNetLog& net_log);
  virtual void AddTLSIntolerantServer(const GURL& url);
  virtual bool IsTLSIntolerantServer(const GURL& url) const;

 private:
  class Request;
  class Job;

  LoadState GetLoadState(const Request& request) const;

  void OnStreamReady(const Job* job,
                     const SSLConfig& ssl_config,
                     const ProxyInfo& proxy_info,
                     HttpStream* stream);
  void OnStreamFailed(const Job* job, int result, const SSLConfig& ssl_config);
  void OnCertificateError(const Job* job,
                          int result,
                          const SSLConfig& ssl_config,
                          const SSLInfo& ssl_info);
  void OnNeedsProxyAuth(const Job* job,
                        const HttpResponseInfo& response_info,
                        const SSLConfig& ssl_config,
                        const ProxyInfo& proxy_info,
                        HttpAuthController* auth_controller);
  void OnNeedsClientAuth(const Job* job,
                         const SSLConfig& ssl_config,
                         SSLCertRequestInfo* cert_info);
  void OnHttpsProxyTunnelResponse(const Job* job,
                                  const HttpResponseInfo& response_info,
                                  const SSLConfig& ssl_config,
                                  const ProxyInfo& proxy_info,
                                  HttpStream* stream);

  void OnPreconnectsComplete(const Job* job);

  // Called when the Preconnect completes. Used for testing.
  virtual void OnPreconnectsCompleteInternal() {}

  HttpNetworkSession* const session_;

  std::set<std::string> tls_intolerant_servers_;

  // All Requests are handed out to clients. By the time HttpStreamFactoryImpl
  // is destroyed, all Requests should be deleted (which should remove them from
  // |request_map_|.
  // TODO(willchan): Change to a different key when we enable late binding. This
  // is the key part that keeps things tightly bound.
  // Note that currently the Request assumes ownership of the Job. We will break
  // this with late binding, and the factory will own the Job.
  std::map<const Job*, Request*> request_map_;

  // These jobs correspond to preconnect requests and have no associated Request
  // object. They're owned by HttpStreamFactoryImpl. Leftover jobs will be
  // deleted when the factory is destroyed.
  std::set<const Job*> preconnect_job_set_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamFactoryImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_IMPL_H_
