// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "net/base/auth.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;

class SSLClientSocketPoolTest : public ClientSocketPoolTest {
 protected:
  SSLClientSocketPoolTest()
      : direct_tcp_socket_params_(new TCPSocketParams(
            HostPortPair("host", 443), MEDIUM, GURL(), false)),
        tcp_socket_pool_(new MockTCPClientSocketPool(
            kMaxSockets,
            kMaxSocketsPerGroup,
            make_scoped_refptr(new ClientSocketPoolHistograms("MockTCP")),
            &socket_factory_)),
        proxy_tcp_socket_params_(new TCPSocketParams(
            HostPortPair("proxy", 443), MEDIUM, GURL(), false)),
        http_proxy_socket_pool_(new HttpProxyClientSocketPool(
            kMaxSockets,
            kMaxSocketsPerGroup,
            make_scoped_refptr(new ClientSocketPoolHistograms("MockHttpProxy")),
            new MockHostResolver,
            tcp_socket_pool_,
            NULL)),
        socks_socket_params_(new SOCKSSocketParams(
            proxy_tcp_socket_params_, true, HostPortPair("sockshost", 443),
            MEDIUM, GURL())),
        socks_socket_pool_(new MockSOCKSClientSocketPool(
            kMaxSockets,
            kMaxSocketsPerGroup,
            make_scoped_refptr(new ClientSocketPoolHistograms("MockSOCKS")),
            tcp_socket_pool_)) {
    scoped_refptr<SSLConfigService> ssl_config_service(
        new SSLConfigServiceDefaults);
    ssl_config_service->GetSSLConfig(&ssl_config_);
  }

  void CreatePool(bool tcp_pool, bool http_proxy_pool, bool socks_pool) {
    pool_ = new SSLClientSocketPool(
        kMaxSockets,
        kMaxSocketsPerGroup,
        make_scoped_refptr(new ClientSocketPoolHistograms("SSLUnitTest")),
        NULL,
        &socket_factory_,
        tcp_pool ? tcp_socket_pool_ : NULL,
        http_proxy_pool ? http_proxy_socket_pool_ : NULL,
        socks_pool ? socks_socket_pool_ : NULL,
        NULL);
  }

  scoped_refptr<SSLSocketParams> SSLParams(
      ProxyServer::Scheme proxy, struct MockHttpAuthControllerData* auth_data,
      size_t auth_data_len, bool want_spdy) {

    scoped_refptr<HttpProxySocketParams> http_proxy_params;
    if (proxy == ProxyServer::SCHEME_HTTP) {
      scoped_refptr<MockHttpAuthController> auth_controller =
         new MockHttpAuthController();
      auth_controller->SetMockAuthControllerData(auth_data, auth_data_len);
      http_proxy_params = new HttpProxySocketParams(proxy_tcp_socket_params_,
                                                    GURL("http://host"),
                                                    HostPortPair("host", 80),
                                                    auth_controller, true);
    }

    return make_scoped_refptr(new SSLSocketParams(
        proxy == ProxyServer::SCHEME_DIRECT ? direct_tcp_socket_params_ : NULL,
        http_proxy_params,
        proxy == ProxyServer::SCHEME_SOCKS5 ? socks_socket_params_ : NULL,
        proxy,
        "host",
        ssl_config_,
        0,
        want_spdy));
  }

  MockClientSocketFactory socket_factory_;

  scoped_refptr<TCPSocketParams> direct_tcp_socket_params_;
  scoped_refptr<MockTCPClientSocketPool> tcp_socket_pool_;

  scoped_refptr<TCPSocketParams> proxy_tcp_socket_params_;
  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_;
  scoped_refptr<HttpProxyClientSocketPool> http_proxy_socket_pool_;

  scoped_refptr<SOCKSSocketParams> socks_socket_params_;
  scoped_refptr<MockSOCKSClientSocketPool> socks_socket_pool_;

  SSLConfig ssl_config_;
  scoped_refptr<SSLClientSocketPool> pool_;
};

TEST_F(SSLClientSocketPoolTest, TCPFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(false, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  int rv = handle.Init("a", params, MEDIUM, NULL, pool_, BoundNetLog());
  EXPECT_EQ(ERR_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, TCPFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(true, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, BasicDirect) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(false, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(false, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, BasicDirectAsync) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, DirectCertError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, ERR_CERT_COMMON_NAME_INVALID);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, DirectSSLError) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, ERR_SSL_PROTOCOL_ERROR);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_TRUE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, DirectWithNPN) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, OK);
  ssl.next_proto_status = SSLClientSocket::kNextProtoNegotiated;
  ssl.next_proto = "http/1.1";
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->wasNpnNegotiated());
}

TEST_F(SSLClientSocketPoolTest, DirectNoSPDY) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, OK);
  ssl.next_proto_status = SSLClientSocket::kNextProtoNegotiated;
  ssl.next_proto = "http/1.1";
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, true);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_NPN_NEGOTIATION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_TRUE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, DirectGotSPDY) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, OK);
  ssl.next_proto_status = SSLClientSocket::kNextProtoNegotiated;
  ssl.next_proto = "spdy/1";
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, true);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->wasNpnNegotiated());
  std::string proto;
  ssl_socket->GetNextProto(&proto);
  EXPECT_EQ(SSLClientSocket::NextProtoFromString(proto),
            SSLClientSocket::kProtoSPDY1);
}

TEST_F(SSLClientSocketPoolTest, DirectGotBonusSPDY) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, OK);
  ssl.next_proto_status = SSLClientSocket::kNextProtoNegotiated;
  ssl.next_proto = "spdy/1";
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->wasNpnNegotiated());
  std::string proto;
  ssl_socket->GetNextProto(&proto);
  EXPECT_EQ(SSLClientSocket::NextProtoFromString(proto),
            SSLClientSocket::kProtoSPDY1);
}

TEST_F(SSLClientSocketPoolTest, SOCKSFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(false, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, SOCKSFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(true, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, SOCKSBasic) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(false, OK));
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(false, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, SOCKSBasicAsync) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, HttpProxyFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(false, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, HttpProxyFailAsync) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(true, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    NULL, 0, false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
}

TEST_F(SSLClientSocketPoolTest, HttpProxyBasic) {
  MockWrite writes[] = {
      MockWrite(false,
                "CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJheg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead(false, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  data.set_connect_data(MockConnect(false, OK));
  socket_factory_.AddSocketDataProvider(&data);
  MockHttpAuthControllerData auth_data[] = {
      MockHttpAuthControllerData("Proxy-Authorization: Basic Zm9vOmJheg=="),
  };
  SSLSocketDataProvider ssl(false, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    auth_data,
                                                    arraysize(auth_data),
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, HttpProxyBasicAsync) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJheg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  socket_factory_.AddSocketDataProvider(&data);
  MockHttpAuthControllerData auth_data[] = {
      MockHttpAuthControllerData("Proxy-Authorization: Basic Zm9vOmJheg=="),
  };
  SSLSocketDataProvider ssl(true, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    auth_data,
                                                    arraysize(auth_data),
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, NeedProxyAuth) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10\r\n\r\n"),
      MockRead("0123456789"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  socket_factory_.AddSocketDataProvider(&data);
  MockHttpAuthControllerData auth_data[] = {
      MockHttpAuthControllerData(""),
  };
  SSLSocketDataProvider ssl(true, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    auth_data,
                                                    arraysize(auth_data),
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  const HttpResponseInfo& tunnel_info = handle.ssl_error_response_info();
  EXPECT_EQ(tunnel_info.headers->response_code(), 407);
}

TEST_F(SSLClientSocketPoolTest, DoProxyAuth) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJheg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10\r\n\r\n"),
      MockRead("0123456789"),
      MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  socket_factory_.AddSocketDataProvider(&data);
  MockHttpAuthControllerData auth_data[] = {
      MockHttpAuthControllerData(""),
      MockHttpAuthControllerData("Proxy-Authorization: Basic Zm9vOmJheg=="),
  };
  SSLSocketDataProvider ssl(true, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    auth_data,
                                                    arraysize(auth_data),
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  const HttpResponseInfo& tunnel_info = handle.ssl_error_response_info();
  EXPECT_EQ(tunnel_info.headers->response_code(), 407);

  params->http_proxy_params()->auth_controller()->ResetAuth(std::wstring(),
                                                            std::wstring());
  rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  // Test that http://crbug.com/49325 doesn't regress.
  EXPECT_EQ(handle.GetLoadState(), LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, DoProxyAuthNoKeepAlive) {
  MockWrite writes1[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockWrite writes2[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJheg==\r\n\r\n"),
  };
  MockRead reads1[] = {
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n\r\n"),
      MockRead("Content0123456789"),
  };
  MockRead reads2[] = {
      MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data1(reads1, arraysize(reads1), writes1,
                                arraysize(writes1));
  socket_factory_.AddSocketDataProvider(&data1);
  StaticSocketDataProvider data2(reads2, arraysize(reads2), writes2,
                                arraysize(writes2));
  socket_factory_.AddSocketDataProvider(&data2);
  MockHttpAuthControllerData auth_data[] = {
      MockHttpAuthControllerData(""),
      MockHttpAuthControllerData("Proxy-Authorization: Basic Zm9vOmJheg=="),
  };
  SSLSocketDataProvider ssl(true, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    auth_data,
                                                    arraysize(auth_data),
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  const HttpResponseInfo& tunnel_info = handle.ssl_error_response_info();
  EXPECT_EQ(tunnel_info.headers->response_code(), 407);

  params->http_proxy_params()->auth_controller()->ResetAuth(std::wstring(),
                                                            std::wstring());
  rv = handle.Init("a", params, MEDIUM, &callback, pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

// It would be nice to also test the timeouts in SSLClientSocketPool.

}  // namespace

}  // namespace net
