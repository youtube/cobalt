// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_factory.h"

#include <string>

#include "base/basictypes.h"
#include "net/base/cert_verifier.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_session_peer.h"
#include "net/http/http_request_info.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct SessionDependencies {
  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : host_resolver(new MockHostResolver),
        cert_verifier(new CertVerifier),
        proxy_service(proxy_service),
        ssl_config_service(new SSLConfigServiceDefaults),
        http_auth_handler_factory(
            HttpAuthHandlerFactory::CreateDefault(host_resolver.get())),
        net_log(NULL) {}

  scoped_ptr<MockHostResolverBase> host_resolver;
  scoped_ptr<CertVerifier> cert_verifier;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  NetLog* net_log;
};

HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(session_deps->host_resolver.get(),
                                session_deps->cert_verifier.get(),
                                NULL /* dnsrr_resolver */,
                                NULL /* dns_cert_checker */,
                                NULL /* ssl_host_info_factory */,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service,
                                new SpdySessionPool(NULL),
                                session_deps->http_auth_handler_factory.get(),
                                NULL,
                                session_deps->net_log);
}

struct TestCase {
  int num_streams;
  bool ssl;
};

TestCase kTests[] = {
  { 1, false },
  { 2, false },
  { 1, true},
  { 2, true},
};

int PreconnectHelper(const TestCase& test,
                     HttpNetworkSession* session) {
  SSLConfig ssl_config;
  session->ssl_config_service()->GetSSLConfig(&ssl_config);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = test.ssl ?  GURL("https://www.google.com") :
      GURL("http://www.google.com");
  request.load_flags = 0;

  ProxyInfo proxy_info;
  TestCompletionCallback callback;

  int rv = session->http_stream_factory()->PreconnectStreams(
      test.num_streams, &request, &ssl_config, &proxy_info, session,
      BoundNetLog(), &callback);
  if (rv != ERR_IO_PENDING)
    return ERR_UNEXPECTED;
  return callback.WaitForResult();
};

template<typename ParentPool>
class CapturePreconnectsSocketPool : public ParentPool {
 public:
  explicit CapturePreconnectsSocketPool(HttpNetworkSession* session);

  int last_num_streams() const {
    return last_num_streams_;
  }

  virtual int RequestSocket(const std::string& group_name,
                            const void* socket_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback,
                            const BoundNetLog& net_log) {
    ADD_FAILURE();
    return ERR_UNEXPECTED;
  }

  virtual void RequestSockets(const std::string& group_name,
                              const void* socket_params,
                              int num_sockets,
                              const BoundNetLog& net_log) {
    last_num_streams_ = num_sockets;
  }

  virtual void CancelRequest(const std::string& group_name,
                             ClientSocketHandle* handle) {
    ADD_FAILURE();
  }
  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket) {
    ADD_FAILURE();
  }
  virtual void CloseIdleSockets() {
    ADD_FAILURE();
  }
  virtual int IdleSocketCount() const {
    ADD_FAILURE();
    return 0;
  }
  virtual int IdleSocketCountInGroup(const std::string& group_name) const {
    ADD_FAILURE();
    return 0;
  }
  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const {
    ADD_FAILURE();
    return LOAD_STATE_IDLE;
  }
  virtual base::TimeDelta ConnectionTimeout() const {
    return base::TimeDelta();
  }

 private:
  int last_num_streams_;
};

typedef CapturePreconnectsSocketPool<TCPClientSocketPool>
CapturePreconnectsTCPSocketPool;
typedef CapturePreconnectsSocketPool<HttpProxyClientSocketPool>
CapturePreconnectsHttpProxySocketPool;
typedef CapturePreconnectsSocketPool<SOCKSClientSocketPool>
CapturePreconnectsSOCKSSocketPool;
typedef CapturePreconnectsSocketPool<SSLClientSocketPool>
CapturePreconnectsSSLSocketPool;

template<typename ParentPool>
CapturePreconnectsSocketPool<ParentPool>::CapturePreconnectsSocketPool(
    HttpNetworkSession* session)
    : ParentPool(0, 0, NULL, session->host_resolver(), NULL, NULL),
      last_num_streams_(-1) {}

template<>
CapturePreconnectsHttpProxySocketPool::CapturePreconnectsSocketPool(
    HttpNetworkSession* session)
    : HttpProxyClientSocketPool(0, 0, NULL, session->host_resolver(), NULL,
                                NULL, NULL),
      last_num_streams_(-1) {}

template<>
CapturePreconnectsSSLSocketPool::CapturePreconnectsSocketPool(
    HttpNetworkSession* session)
    : SSLClientSocketPool(0, 0, NULL, session->host_resolver(),
                          session->cert_verifier(), NULL, NULL,
                          NULL, NULL, NULL, NULL, NULL, NULL, NULL),
      last_num_streams_(-1) {}

TEST(HttpStreamFactoryTest, PreconnectDirect) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SessionDependencies session_deps(ProxyService::CreateDirect());
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    CapturePreconnectsTCPSocketPool* tcp_conn_pool =
        new CapturePreconnectsTCPSocketPool(session);
    peer.SetTCPSocketPool(tcp_conn_pool);
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(session.get());
    peer.SetSSLSocketPool(ssl_conn_pool);
    EXPECT_EQ(OK, PreconnectHelper(kTests[i], session));
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, tcp_conn_pool->last_num_streams());
  }
}

TEST(HttpStreamFactoryTest, PreconnectHttpProxy) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SessionDependencies session_deps(ProxyService::CreateFixed("http_proxy"));
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    HostPortPair proxy_host("http_proxy", 80);
    CapturePreconnectsHttpProxySocketPool* http_proxy_pool =
        new CapturePreconnectsHttpProxySocketPool(session);
    peer.SetSocketPoolForHTTPProxy(proxy_host, http_proxy_pool);
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(session);
    peer.SetSocketPoolForSSLWithProxy(proxy_host, ssl_conn_pool);
    EXPECT_EQ(OK, PreconnectHelper(kTests[i], session));
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, http_proxy_pool->last_num_streams());
  }
}

TEST(HttpStreamFactoryTest, PreconnectSocksProxy) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SessionDependencies session_deps(
        ProxyService::CreateFixed("socks4://socks_proxy:1080"));
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);
    HostPortPair proxy_host("socks_proxy", 1080);
    CapturePreconnectsSOCKSSocketPool* socks_proxy_pool =
        new CapturePreconnectsSOCKSSocketPool(session);
    peer.SetSocketPoolForSOCKSProxy(proxy_host, socks_proxy_pool);
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(session);
    peer.SetSocketPoolForSSLWithProxy(proxy_host, ssl_conn_pool);
    EXPECT_EQ(OK, PreconnectHelper(kTests[i], session));
    if (kTests[i].ssl)
      EXPECT_EQ(kTests[i].num_streams, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, socks_proxy_pool->last_num_streams());
  }
}

TEST(HttpStreamFactoryTest, PreconnectDirectWithExistingSpdySession) {
  for (size_t i = 0; i < arraysize(kTests); ++i) {
    SessionDependencies session_deps(ProxyService::CreateDirect());
    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    HttpNetworkSessionPeer peer(session);

    // Set an existing SpdySession in the pool.
    HostPortPair host_port_pair("www.google.com", 443);
    HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
    scoped_refptr<SpdySession> spdy_session =
        session->spdy_session_pool()->Get(
            pair, session->mutable_spdy_settings(), BoundNetLog());

    CapturePreconnectsTCPSocketPool* tcp_conn_pool =
        new CapturePreconnectsTCPSocketPool(session);
    peer.SetTCPSocketPool(tcp_conn_pool);
    CapturePreconnectsSSLSocketPool* ssl_conn_pool =
        new CapturePreconnectsSSLSocketPool(session.get());
    peer.SetSSLSocketPool(ssl_conn_pool);
    EXPECT_EQ(OK, PreconnectHelper(kTests[i], session));
    // We shouldn't be preconnecting if we have an existing session, which is
    // the case for https://www.google.com.
    if (kTests[i].ssl)
      EXPECT_EQ(-1, ssl_conn_pool->last_num_streams());
    else
      EXPECT_EQ(kTests[i].num_streams, tcp_conn_pool->last_num_streams());
  }
}

}  // namespace

}  // namespace net
