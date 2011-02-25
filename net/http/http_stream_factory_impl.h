// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_IMPL_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_IMPL_H_

#include <map>
#include <set>
#include <string>

#include "base/ref_counted.h"
#include "net/http/http_stream_factory.h"
#include "net/proxy/proxy_server.h"

namespace net {

class HttpNetworkSession;
class SpdySession;

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

  typedef std::set<Request*> RequestSet;
  typedef std::map<HostPortProxyPair, RequestSet> SpdySessionRequestMap;

  LoadState GetLoadState(const Request& request) const;

  // Called when a SpdySession is ready. It will find appropriate Requests and
  // fulfill them. |direct| indicates whether or not |spdy_session| uses a
  // proxy.
  void OnSpdySessionReady(const Job* job,
                          scoped_refptr<SpdySession> spdy_session,
                          bool direct);

  // Called when the Job detects that the endpoint indicated by the
  // Alternate-Protocol does not work. Lets the factory update
  // HttpAlternateProtocols with the failure and resets the SPDY session key.
  void OnBrokenAlternateProtocol(const Job*, const HostPortPair& origin);

  // Invoked when the Job finishes preconnecting sockets.
  void OnPreconnectsComplete(const Job* job);

  // Called when the Preconnect completes. Used for testing.
  virtual void OnPreconnectsCompleteInternal() {}

  HttpNetworkSession* const session_;

  std::set<std::string> tls_intolerant_servers_;

  // All Requests are handed out to clients. By the time HttpStreamFactoryImpl
  // is destroyed, all Requests should be deleted (which should remove them from
  // |request_map_|. The Requests will delete the corresponding job.
  std::map<const Job*, Request*> request_map_;

  SpdySessionRequestMap spdy_session_request_map_;

  // These jobs correspond to preconnect requests and have no associated Request
  // object. They're owned by HttpStreamFactoryImpl. Leftover jobs will be
  // deleted when the factory is destroyed.
  std::set<const Job*> preconnect_job_set_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamFactoryImpl);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_IMPL_H_
