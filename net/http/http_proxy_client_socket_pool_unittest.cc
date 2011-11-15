// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;
const char * const kAuthHeaders[] = {
  "proxy-authorization", "Basic Zm9vOmJhcg=="
};
const int kAuthHeadersSize = arraysize(kAuthHeaders) / 2;

enum HttpProxyType {
  HTTP,
  HTTPS,
  SPDY
};

typedef ::testing::TestWithParam<HttpProxyType> TestWithHttpParam;

}  // namespace

class HttpProxyClientSocketPoolTest : public TestWithHttpParam {
 protected:
  HttpProxyClientSocketPoolTest()
      : ssl_config_(),
        ignored_transport_socket_params_(new TransportSocketParams(
            HostPortPair("proxy", 80), LOWEST, false, false)),
        ignored_ssl_socket_params_(new SSLSocketParams(
            ignored_transport_socket_params_, NULL, NULL,
            ProxyServer::SCHEME_DIRECT, HostPortPair("www.google.com", 443),
            ssl_config_, 0, false, false)),
        tcp_histograms_("MockTCP"),
        transport_socket_pool_(
            kMaxSockets, kMaxSocketsPerGroup,
            &tcp_histograms_,
            &socket_factory_),
        ssl_histograms_("MockSSL"),
        proxy_service_(ProxyService::CreateDirect()),
        ssl_config_service_(new SSLConfigServiceDefaults),
        ssl_socket_pool_(kMaxSockets, kMaxSocketsPerGroup,
                         &ssl_histograms_,
                         &host_resolver_,
                         &cert_verifier_,
                         NULL /* origin_bound_cert_store */,
                         NULL /* dnsrr_resolver */,
                         NULL /* dns_cert_checker */,
                         NULL /* ssl_host_info_factory */,
                         &socket_factory_,
                         &transport_socket_pool_,
                         NULL,
                         NULL,
                         ssl_config_service_.get(),
                         BoundNetLog().net_log()),
        http_auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)),
        session_(CreateNetworkSession()),
        http_proxy_histograms_("HttpProxyUnitTest"),
        ssl_data_(NULL),
        data_(NULL),
        pool_(kMaxSockets, kMaxSocketsPerGroup,
              &http_proxy_histograms_,
              NULL,
              &transport_socket_pool_,
              &ssl_socket_pool_,
              NULL) {
  }

  virtual ~HttpProxyClientSocketPoolTest() {
  }

  void AddAuthToCache() {
    const string16 kFoo(ASCIIToUTF16("foo"));
    const string16 kBar(ASCIIToUTF16("bar"));
    GURL proxy_url(GetParam() == HTTP ? "http://proxy" : "https://proxy:80");
    session_->http_auth_cache()->Add(proxy_url,
                                     "MyRealm1",
                                     HttpAuth::AUTH_SCHEME_BASIC,
                                     "Basic realm=MyRealm1",
                                     AuthCredentials(kFoo, kBar),
                                     "/");
  }

  scoped_refptr<TransportSocketParams> GetTcpParams() {
    if (GetParam() != HTTP)
      return scoped_refptr<TransportSocketParams>();
    return ignored_transport_socket_params_;
  }

  scoped_refptr<SSLSocketParams> GetSslParams() {
    if (GetParam() == HTTP)
      return scoped_refptr<SSLSocketParams>();
    return ignored_ssl_socket_params_;
  }

  // Returns the a correctly constructed HttpProxyParms
  // for the HTTP or HTTPS proxy.
  scoped_refptr<HttpProxySocketParams> GetParams(bool tunnel) {
    return scoped_refptr<HttpProxySocketParams>(
        new HttpProxySocketParams(
            GetTcpParams(),
            GetSslParams(),
            GURL(tunnel ? "https://www.google.com/" : "http://www.google.com"),
            "",
            HostPortPair("www.google.com", tunnel ? 443 : 80),
            session_->http_auth_cache(),
            session_->http_auth_handler_factory(),
            session_->spdy_session_pool(),
            tunnel));
  }

  scoped_refptr<HttpProxySocketParams> GetTunnelParams() {
    return GetParams(true);
  }

  scoped_refptr<HttpProxySocketParams> GetNoTunnelParams() {
    return GetParams(false);
  }

  DeterministicMockClientSocketFactory& socket_factory() {
    return socket_factory_;
  }

  void Initialize(bool async, MockRead* reads, size_t reads_count,
                  MockWrite* writes, size_t writes_count,
                  MockRead* spdy_reads, size_t spdy_reads_count,
                  MockWrite* spdy_writes, size_t spdy_writes_count) {
    if (GetParam() == SPDY)
      data_ = new DeterministicSocketData(spdy_reads, spdy_reads_count,
                                          spdy_writes, spdy_writes_count);
    else
      data_ = new DeterministicSocketData(reads, reads_count, writes,
                                          writes_count);

    data_->set_connect_data(MockConnect(async, 0));
    data_->StopAfter(2);  // Request / Response

    socket_factory_.AddSocketDataProvider(data_.get());

    if (GetParam() != HTTP) {
      ssl_data_.reset(new SSLSocketDataProvider(async, OK));
      if (GetParam() == SPDY) {
        InitializeSpdySsl();
      }
      socket_factory_.AddSSLSocketDataProvider(ssl_data_.get());
    }
  }

  void InitializeSpdySsl() {
    spdy::SpdyFramer::set_enable_compression_default(false);
    ssl_data_->next_proto_status = SSLClientSocket::kNextProtoNegotiated;
    ssl_data_->next_proto = "spdy/2";
    ssl_data_->was_npn_negotiated = true;
  }

  HttpNetworkSession* CreateNetworkSession() {
    HttpNetworkSession::Params params;
    params.host_resolver = &host_resolver_;
    params.cert_verifier = &cert_verifier_;
    params.proxy_service = proxy_service_.get();
    params.client_socket_factory = &socket_factory_;
    params.ssl_config_service = ssl_config_service_;
    params.http_auth_handler_factory = http_auth_handler_factory_.get();
    params.http_server_properties = &http_server_properties_;
    return new HttpNetworkSession(params);
  }

 private:
  SSLConfig ssl_config_;

  scoped_refptr<TransportSocketParams> ignored_transport_socket_params_;
  scoped_refptr<SSLSocketParams> ignored_ssl_socket_params_;
  ClientSocketPoolHistograms tcp_histograms_;
  DeterministicMockClientSocketFactory socket_factory_;
  MockTransportClientSocketPool transport_socket_pool_;
  ClientSocketPoolHistograms ssl_histograms_;
  MockHostResolver host_resolver_;
  CertVerifier cert_verifier_;
  const scoped_ptr<ProxyService> proxy_service_;
  const scoped_refptr<SSLConfigService> ssl_config_service_;
  SSLClientSocketPool ssl_socket_pool_;

  const scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory_;
  HttpServerPropertiesImpl http_server_properties_;
  const scoped_refptr<HttpNetworkSession> session_;
  ClientSocketPoolHistograms http_proxy_histograms_;

 protected:
  scoped_ptr<SSLSocketDataProvider> ssl_data_;
  scoped_refptr<DeterministicSocketData> data_;
  HttpProxyClientSocketPool pool_;
  ClientSocketHandle handle_;
  TestOldCompletionCallback callback_;
};

//-----------------------------------------------------------------------------
// All tests are run with three different proxy types: HTTP, HTTPS (non-SPDY)
// and SPDY.
INSTANTIATE_TEST_CASE_P(HttpProxyClientSocketPoolTests,
                        HttpProxyClientSocketPoolTest,
                        ::testing::Values(HTTP, HTTPS, SPDY));

TEST_P(HttpProxyClientSocketPoolTest, NoTunnel) {
  Initialize(false, NULL, 0, NULL, 0, NULL, 0, NULL, 0);

  int rv = handle_.Init("a", GetNoTunnelParams(), LOW, NULL, &pool_,
                       BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle_.socket());
  EXPECT_TRUE(tunnel_socket->IsConnected());
}

TEST_P(HttpProxyClientSocketPoolTest, NeedAuth) {
  MockWrite writes[] = {
    MockWrite(true, 0, "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };
  MockRead reads[] = {
    // No credentials.
    MockRead(true, 1, "HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead(true, 2, "Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead(true, 3, "Content-Length: 10\r\n\r\n"),
    MockRead(true, 4, "0123456789"),
  };
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyConnect(NULL, 0, 1));
  scoped_ptr<spdy::SpdyFrame> rst(ConstructSpdyRstStream(1, spdy::CANCEL));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, true),
    CreateMockWrite(*rst, 2, true),
  };
  scoped_ptr<spdy::SpdyFrame> resp(
      ConstructSpdySynReplyError(
          "407 Proxy Authentication Required", NULL, 0, 1));
  MockRead spdy_reads[] = {
    CreateMockWrite(*resp, 1, true),
    MockRead(true, 0, 3)
  };

  Initialize(false, reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));

  data_->StopAfter(4);
  int rv = handle_.Init("a", GetTunnelParams(), LOW, &callback_, &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  data_->RunFor(4);
  rv = callback_.WaitForResult();
  if (GetParam() != SPDY) {
    EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, rv);
    EXPECT_TRUE(handle_.is_initialized());
    ASSERT_TRUE(handle_.socket());
    HttpProxyClientSocket* tunnel_socket =
            static_cast<HttpProxyClientSocket*>(handle_.socket());
    EXPECT_FALSE(tunnel_socket->IsConnected());
    EXPECT_FALSE(tunnel_socket->using_spdy());
  } else {
    // Proxy auth is not really implemented for SPDY yet
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);
    EXPECT_FALSE(handle_.is_initialized());
    EXPECT_FALSE(handle_.socket());
  }
}

TEST_P(HttpProxyClientSocketPoolTest, HaveAuth) {
  // It's pretty much impossible to make the SPDY case becave synchronously
  // so we skip this test for SPDY
  if (GetParam() == SPDY)
    return;
  MockWrite writes[] = {
    MockWrite(false, 0,
              "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, 1, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  Initialize(false, reads, arraysize(reads), writes, arraysize(writes), NULL, 0,
             NULL, 0);
  AddAuthToCache();

  int rv = handle_.Init("a", GetTunnelParams(), LOW, &callback_, &pool_,
                       BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle_.socket());
  EXPECT_TRUE(tunnel_socket->IsConnected());
}

TEST_P(HttpProxyClientSocketPoolTest, AsyncHaveAuth) {
  MockWrite writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(false, "HTTP/1.1 200 Connection Established\r\n\r\n"),
  };

  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyConnect(kAuthHeaders,
                                                       kAuthHeadersSize, 1));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, true)
  };
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 2)
  };

  Initialize(false, reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));
  AddAuthToCache();

  int rv = handle_.Init("a", GetTunnelParams(), LOW, &callback_, &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  data_->RunFor(2);
  EXPECT_EQ(OK, callback_.WaitForResult());
  EXPECT_TRUE(handle_.is_initialized());
  ASSERT_TRUE(handle_.socket());
  HttpProxyClientSocket* tunnel_socket =
          static_cast<HttpProxyClientSocket*>(handle_.socket());
  EXPECT_TRUE(tunnel_socket->IsConnected());
}

TEST_P(HttpProxyClientSocketPoolTest, TCPError) {
  if (GetParam() == SPDY) return;
  data_ = new DeterministicSocketData(NULL, 0, NULL, 0);
  data_->set_connect_data(MockConnect(true, ERR_CONNECTION_CLOSED));

  socket_factory().AddSocketDataProvider(data_.get());

  int rv = handle_.Init("a", GetTunnelParams(), LOW, &callback_, &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, callback_.WaitForResult());

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, SSLError) {
  if (GetParam() == HTTP) return;
  data_ = new DeterministicSocketData(NULL, 0, NULL, 0);
  data_->set_connect_data(MockConnect(true, OK));
  socket_factory().AddSocketDataProvider(data_.get());

  ssl_data_.reset(new SSLSocketDataProvider(true,
                                            ERR_CERT_AUTHORITY_INVALID));
  if (GetParam() == SPDY) {
    InitializeSpdySsl();
  }
  socket_factory().AddSSLSocketDataProvider(ssl_data_.get());

  int rv = handle_.Init("a", GetTunnelParams(), LOW, &callback_, &pool_,
                        BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_EQ(ERR_PROXY_CERTIFICATE_INVALID, callback_.WaitForResult());

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, SslClientAuth) {
  if (GetParam() == HTTP) return;
  data_ = new DeterministicSocketData(NULL, 0, NULL, 0);
  data_->set_connect_data(MockConnect(true, OK));
  socket_factory().AddSocketDataProvider(data_.get());

  ssl_data_.reset(new SSLSocketDataProvider(true,
                                            ERR_SSL_CLIENT_AUTH_CERT_NEEDED));
  if (GetParam() == SPDY) {
    InitializeSpdySsl();
  }
  socket_factory().AddSSLSocketDataProvider(ssl_data_.get());

  int rv = handle_.Init("a", GetTunnelParams(), LOW, &callback_, &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  EXPECT_EQ(ERR_SSL_CLIENT_AUTH_CERT_NEEDED, callback_.WaitForResult());

  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, TunnelUnexpectedClose) {
  MockWrite writes[] = {
    MockWrite(true, 0,
              "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(true, 1, "HTTP/1.1 200 Conn"),
    MockRead(true, ERR_CONNECTION_CLOSED, 2),
  };
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyConnect(kAuthHeaders,
                                                       kAuthHeadersSize, 1));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, true)
  };
  MockRead spdy_reads[] = {
    MockRead(true, ERR_CONNECTION_CLOSED, 1),
  };

  Initialize(false, reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));
  AddAuthToCache();

  int rv = handle_.Init("a", GetTunnelParams(), LOW, &callback_, &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  data_->RunFor(3);
  EXPECT_EQ(ERR_CONNECTION_CLOSED, callback_.WaitForResult());
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());
}

TEST_P(HttpProxyClientSocketPoolTest, TunnelSetupError) {
  MockWrite writes[] = {
    MockWrite(true, 0,
              "CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };
  MockRead reads[] = {
    MockRead(true, 1, "HTTP/1.1 304 Not Modified\r\n\r\n"),
  };
  scoped_ptr<spdy::SpdyFrame> req(ConstructSpdyConnect(kAuthHeaders,
                                                       kAuthHeadersSize, 1));
  scoped_ptr<spdy::SpdyFrame> rst(ConstructSpdyRstStream(1, spdy::CANCEL));
  MockWrite spdy_writes[] = {
    CreateMockWrite(*req, 0, true),
    CreateMockWrite(*rst, 2, true),
  };
  scoped_ptr<spdy::SpdyFrame> resp(ConstructSpdySynReplyError(1));
  MockRead spdy_reads[] = {
    CreateMockRead(*resp, 1, true),
    MockRead(true, 0, 3),
  };

  Initialize(false, reads, arraysize(reads), writes, arraysize(writes),
             spdy_reads, arraysize(spdy_reads), spdy_writes,
             arraysize(spdy_writes));
  AddAuthToCache();

  int rv = handle_.Init("a", GetTunnelParams(), LOW, &callback_, &pool_,
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle_.is_initialized());
  EXPECT_FALSE(handle_.socket());

  data_->RunFor(2);

  rv = callback_.WaitForResult();
  if (GetParam() == HTTP) {
    // HTTP Proxy CONNECT responses are not trustworthy
    EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);
    EXPECT_FALSE(handle_.is_initialized());
    EXPECT_FALSE(handle_.socket());
  } else {
    // HTTPS or SPDY Proxy CONNECT responses are trustworthy
    EXPECT_EQ(ERR_HTTPS_PROXY_TUNNEL_RESPONSE, rv);
    EXPECT_TRUE(handle_.is_initialized());
    EXPECT_TRUE(handle_.socket());
  }
}

// It would be nice to also test the timeouts in HttpProxyClientSocketPool.

}  // namespace net
