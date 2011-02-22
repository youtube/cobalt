// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/base/auth.h"
#include "net/base/cert_verifier.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;

class SSLClientSocketPoolTest : public testing::Test {
 protected:
  SSLClientSocketPoolTest()
      : proxy_service_(ProxyService::CreateDirect()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        http_auth_handler_factory_(HttpAuthHandlerFactory::CreateDefault(
            &host_resolver_)),
        session_(CreateNetworkSession()),
        direct_tcp_socket_params_(new TCPSocketParams(
            HostPortPair("host", 443), MEDIUM, GURL(), false)),
        tcp_histograms_("MockTCP"),
        tcp_socket_pool_(
            kMaxSockets,
            kMaxSocketsPerGroup,
            &tcp_histograms_,
            &socket_factory_),
        proxy_tcp_socket_params_(new TCPSocketParams(
            HostPortPair("proxy", 443), MEDIUM, GURL(), false)),
        socks_socket_params_(new SOCKSSocketParams(
            proxy_tcp_socket_params_, true, HostPortPair("sockshost", 443),
            MEDIUM, GURL())),
        socks_histograms_("MockSOCKS"),
        socks_socket_pool_(
            kMaxSockets,
            kMaxSocketsPerGroup,
            &socks_histograms_,
            &tcp_socket_pool_),
        http_proxy_socket_params_(new HttpProxySocketParams(
            proxy_tcp_socket_params_, NULL, GURL("http://host"), "",
            HostPortPair("host", 80),
            session_->http_auth_cache(),
            session_->http_auth_handler_factory(),
            session_->spdy_session_pool(),
            true)),
        http_proxy_histograms_("MockHttpProxy"),
        http_proxy_socket_pool_(
            kMaxSockets,
            kMaxSocketsPerGroup,
            &http_proxy_histograms_,
            &host_resolver_,
            &tcp_socket_pool_,
            NULL,
            NULL) {
    scoped_refptr<SSLConfigService> ssl_config_service(
        new SSLConfigServiceDefaults);
    ssl_config_service->GetSSLConfig(&ssl_config_);
  }

  void CreatePool(bool tcp_pool, bool http_proxy_pool, bool socks_pool) {
    ssl_histograms_.reset(new ClientSocketPoolHistograms("SSLUnitTest"));
    pool_.reset(new SSLClientSocketPool(
        kMaxSockets,
        kMaxSocketsPerGroup,
        ssl_histograms_.get(),
        NULL /* host_resolver */,
        NULL /* cert_verifier */,
        NULL /* dnsrr_resolver */,
        NULL /* dns_cert_checker */,
        NULL /* ssl_host_info_factory */,
        &socket_factory_,
        tcp_pool ? &tcp_socket_pool_ : NULL,
        socks_pool ? &socks_socket_pool_ : NULL,
        http_proxy_pool ? &http_proxy_socket_pool_ : NULL,
        NULL,
        NULL));
  }

  scoped_refptr<SSLSocketParams> SSLParams(ProxyServer::Scheme proxy,
                                           bool want_spdy_over_npn) {
    return make_scoped_refptr(new SSLSocketParams(
        proxy == ProxyServer::SCHEME_DIRECT ? direct_tcp_socket_params_ : NULL,
        proxy == ProxyServer::SCHEME_SOCKS5 ? socks_socket_params_ : NULL,
        proxy == ProxyServer::SCHEME_HTTP ? http_proxy_socket_params_ : NULL,
        proxy,
        HostPortPair("host", 443),
        ssl_config_,
        0,
        false,
        want_spdy_over_npn));
  }

  void AddAuthToCache() {
    const string16 kFoo(ASCIIToUTF16("foo"));
    const string16 kBar(ASCIIToUTF16("bar"));
    session_->http_auth_cache()->Add(GURL("http://proxy:443/"),
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     kFoo,
                                     kBar,
                                     "/");
  }

  HttpNetworkSession* CreateNetworkSession() {
    HttpNetworkSession::Params params;
    params.host_resolver = &host_resolver_;
    params.cert_verifier = &cert_verifier_;
    params.proxy_service = proxy_service_;
    params.client_socket_factory = &socket_factory_;
    params.ssl_config_service = ssl_config_service_;
    params.http_auth_handler_factory = http_auth_handler_factory_.get();
    return new HttpNetworkSession(params);
  }

  MockClientSocketFactory socket_factory_;
  MockHostResolver host_resolver_;
  CertVerifier cert_verifier_;
  const scoped_refptr<ProxyService> proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  const scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  const scoped_refptr<HttpNetworkSession> session_;

  scoped_refptr<TCPSocketParams> direct_tcp_socket_params_;
  ClientSocketPoolHistograms tcp_histograms_;
  MockTCPClientSocketPool tcp_socket_pool_;

  scoped_refptr<TCPSocketParams> proxy_tcp_socket_params_;

  scoped_refptr<SOCKSSocketParams> socks_socket_params_;
  ClientSocketPoolHistograms socks_histograms_;
  MockSOCKSClientSocketPool socks_socket_pool_;

  scoped_refptr<HttpProxySocketParams> http_proxy_socket_params_;
  ClientSocketPoolHistograms http_proxy_histograms_;
  HttpProxyClientSocketPool http_proxy_socket_pool_;

  SSLConfig ssl_config_;
  scoped_ptr<ClientSocketPoolHistograms> ssl_histograms_;
  scoped_ptr<SSLClientSocketPool> pool_;
};

TEST_F(SSLClientSocketPoolTest, TCPFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(false, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    false);

  ClientSocketHandle handle;
  int rv = handle.Init("a", params, MEDIUM, NULL, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->was_npn_negotiated());
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
                                                    true);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
  ssl.next_proto = "spdy/2";
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    true);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->was_npn_negotiated());
  std::string proto;
  ssl_socket->GetNextProto(&proto);
  EXPECT_EQ(SSLClientSocket::NextProtoFromString(proto),
            SSLClientSocket::kProtoSPDY2);
}

TEST_F(SSLClientSocketPoolTest, DirectGotBonusSPDY) {
  StaticSocketDataProvider data;
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(true, OK);
  ssl.next_proto_status = SSLClientSocket::kNextProtoNegotiated;
  ssl.next_proto = "spdy/2";
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(true /* tcp pool */, false, false);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_DIRECT,
                                                    true);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  SSLClientSocket* ssl_socket = static_cast<SSLClientSocket*>(handle.socket());
  EXPECT_TRUE(ssl_socket->was_npn_negotiated());
  std::string proto;
  ssl_socket->GetNextProto(&proto);
  EXPECT_EQ(SSLClientSocket::NextProtoFromString(proto),
            SSLClientSocket::kProtoSPDY2);
}

TEST_F(SSLClientSocketPoolTest, SOCKSFail) {
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(false, ERR_CONNECTION_FAILED));
  socket_factory_.AddSocketDataProvider(&data);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_SOCKS5,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, rv);
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
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, callback.WaitForResult());
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
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead(false, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  data.set_connect_data(MockConnect(false, OK));
  socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();
  SSLSocketDataProvider ssl(false, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SSLClientSocketPoolTest, HttpProxyBasicAsync) {
  MockWrite writes[] = {
      MockWrite("CONNECT host:80 HTTP/1.1\r\n"
                "Host: host\r\n"
                "Proxy-Connection: keep-alive\r\n"
                "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
      MockRead("HTTP/1.1 200 Connection Established\r\n\r\n"),
  };
  StaticSocketDataProvider data(reads, arraysize(reads), writes,
                                arraysize(writes));
  socket_factory_.AddSocketDataProvider(&data);
  AddAuthToCache();
  SSLSocketDataProvider ssl(true, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
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
  SSLSocketDataProvider ssl(true, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  CreatePool(false, true /* http proxy pool */, true /* socks pool */);
  scoped_refptr<SSLSocketParams> params = SSLParams(ProxyServer::SCHEME_HTTP,
                                                    false);

  ClientSocketHandle handle;
  TestCompletionCallback callback;
  int rv = handle.Init(
      "a", params, MEDIUM, &callback, pool_.get(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  const HttpResponseInfo& tunnel_info = handle.ssl_error_response_info();
  EXPECT_EQ(tunnel_info.headers->response_code(), 407);
  scoped_ptr<ClientSocketHandle> tunnel_handle(
      handle.release_pending_http_proxy_connection());
  EXPECT_TRUE(tunnel_handle->socket());
  EXPECT_FALSE(tunnel_handle->socket()->IsConnected());
}

// It would be nice to also test the timeouts in SSLClientSocketPool.

}  // namespace

}  // namespace net
