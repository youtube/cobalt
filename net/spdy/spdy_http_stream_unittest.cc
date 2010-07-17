// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class SpdySessionPoolPeer {
 public:
  explicit SpdySessionPoolPeer(const scoped_refptr<SpdySessionPool>& pool)
      : pool_(pool) {}

  void RemoveSpdySession(const scoped_refptr<SpdySession>& session) {
    pool_->Remove(session);
  }

 private:
  const scoped_refptr<SpdySessionPool> pool_;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPoolPeer);
};

namespace {

// Create a proxy service which fails on all requests (falls back to direct).
ProxyService* CreateNullProxyService() {
  return ProxyService::CreateNull();
}

// Helper to manage the lifetimes of the dependencies for a
// SpdyNetworkTransaction.
class SessionDependencies {
 public:
  // Default set of dependencies -- "null" proxy service.
  SessionDependencies()
      : host_resolver(new MockHostResolver),
        proxy_service(CreateNullProxyService()),
        ssl_config_service(new SSLConfigServiceDefaults),
        http_auth_handler_factory(HttpAuthHandlerFactory::CreateDefault()),
        spdy_session_pool(new SpdySessionPool()) {}

  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : host_resolver(new MockHostResolver),
        proxy_service(proxy_service),
        ssl_config_service(new SSLConfigServiceDefaults),
        http_auth_handler_factory(HttpAuthHandlerFactory::CreateDefault()),
        spdy_session_pool(new SpdySessionPool()) {}

  scoped_refptr<MockHostResolverBase> host_resolver;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  scoped_refptr<SpdySessionPool> spdy_session_pool;
};

HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(session_deps->host_resolver,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service,
                                session_deps->spdy_session_pool,
                                session_deps->http_auth_handler_factory.get(),
                                NULL,
                                NULL);
}

class SpdyHttpStreamTest : public testing::Test {
 protected:
  SpdyHttpStreamTest()
      : session_(CreateSession(&session_deps_)),
        pool_peer_(session_->spdy_session_pool()) {}

  scoped_refptr<SpdySession> CreateSpdySession() {
    HostPortPair host_port_pair("www.google.com", 80);
    scoped_refptr<SpdySession> session(
        session_->spdy_session_pool()->Get(
            host_port_pair, session_, BoundNetLog()));
    return session;
  }

  virtual void TearDown() {
    MessageLoop::current()->RunAllPending();
  }

  SessionDependencies session_deps_;
  scoped_refptr<HttpNetworkSession> session_;
  SpdySessionPoolPeer pool_peer_;
};

// Needs fixing, see http://crbug.com/28622
TEST_F(SpdyHttpStreamTest, SendRequest) {
  scoped_refptr<SpdySession> session(CreateSpdySession());
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  TestCompletionCallback callback;
  HttpResponseInfo response;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream());
  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(session, request, BoundNetLog(), NULL));
  http_stream->InitializeRequest(base::Time::Now(), NULL);
  EXPECT_EQ(ERR_IO_PENDING,
            http_stream->SendRequest(&response, &callback));

  // Need to manually remove the spdy session since normally it gets removed on
  // socket close/error, but we aren't communicating over a socket here.
  pool_peer_.RemoveSpdySession(session);
}

// TODO(willchan): Write a longer test for SpdyStream that exercises all
// methods.

}  // namespace

}  // namespace net
