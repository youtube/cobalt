// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"
#include "base/ref_counted.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class FlipSessionPoolPeer {
 public:
  explicit FlipSessionPoolPeer(const scoped_refptr<FlipSessionPool>& pool)
      : pool_(pool) {}

  void RemoveFlipSession(const scoped_refptr<FlipSession>& session) {
    pool_->Remove(session);
  }

 private:
  const scoped_refptr<FlipSessionPool> pool_;

  DISALLOW_COPY_AND_ASSIGN(FlipSessionPoolPeer);
};

namespace {

// Create a proxy service which fails on all requests (falls back to direct).
ProxyService* CreateNullProxyService() {
  return ProxyService::CreateNull();
}

// Helper to manage the lifetimes of the dependencies for a
// FlipNetworkTransaction.
class SessionDependencies {
 public:
  // Default set of dependencies -- "null" proxy service.
  SessionDependencies()
      : host_resolver(new MockHostResolver),
        proxy_service(CreateNullProxyService()),
        ssl_config_service(new SSLConfigServiceDefaults),
        flip_session_pool(new FlipSessionPool) {}

  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : host_resolver(new MockHostResolver),
        proxy_service(proxy_service),
        ssl_config_service(new SSLConfigServiceDefaults),
        flip_session_pool(new FlipSessionPool) {}

  scoped_refptr<MockHostResolverBase> host_resolver;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_refptr<FlipSessionPool> flip_session_pool;
};

HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(NULL,
                                session_deps->host_resolver,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service,
                                session_deps->flip_session_pool);
}

class FlipStreamTest : public testing::Test {
 protected:
  FlipStreamTest()
      : session_(CreateSession(&session_deps_)),
        pool_peer_(session_->flip_session_pool()) {}

  scoped_refptr<FlipSession> CreateFlipSession() {
    HostResolver::RequestInfo resolve_info("www.google.com", 80);
    scoped_refptr<FlipSession> session(
        session_->flip_session_pool()->Get(resolve_info, session_));
    return session;
  }

  virtual void TearDown() {
    MessageLoop::current()->RunAllPending();
  }

  SessionDependencies session_deps_;
  scoped_refptr<HttpNetworkSession> session_;
  FlipSessionPoolPeer pool_peer_;
};

// Needs fixing, see http://crbug.com/28622
TEST_F(FlipStreamTest, SendRequest) {
  scoped_refptr<FlipSession> session(CreateFlipSession());
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  TestCompletionCallback callback;
  HttpResponseInfo response;

  scoped_refptr<FlipStream> stream(new FlipStream(session, 1, false, NULL));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(NULL, &response, &callback));

  // Need to manually remove the flip session since normally it gets removed on
  // socket close/error, but we aren't communicating over a socket here.
  pool_peer_.RemoveFlipSession(session);
}

// TODO(willchan): Write a longer test for FlipStream that exercises all
// methods.

}  // namespace

}  // namespace net
