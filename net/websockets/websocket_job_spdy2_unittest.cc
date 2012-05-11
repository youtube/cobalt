// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_job.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/test_completion_callback.h"
#include "net/base/transport_security_state.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/socket_stream/socket_stream.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_spdy2.h"
#include "net/spdy/spdy_websocket_test_util_spdy2.h"
#include "net/url_request/url_request_context.h"
#include "net/websockets/websocket_throttle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using namespace net::test_spdy2;

namespace {

class MockSocketStream : public net::SocketStream {
 public:
  MockSocketStream(const GURL& url, net::SocketStream::Delegate* delegate)
      : SocketStream(url, delegate) {}

  virtual void Connect() OVERRIDE {}
  virtual bool SendData(const char* data, int len) OVERRIDE {
    sent_data_ += std::string(data, len);
    return true;
  }

  virtual void Close() OVERRIDE {}
  virtual void RestartWithAuth(
      const net::AuthCredentials& credentials) OVERRIDE {
  }

  virtual void DetachDelegate() OVERRIDE {
    delegate_ = NULL;
  }

  const std::string& sent_data() const {
    return sent_data_;
  }

 protected:
  virtual ~MockSocketStream() {}

 private:
  std::string sent_data_;
};

class MockSocketStreamDelegate : public net::SocketStream::Delegate {
 public:
  MockSocketStreamDelegate()
      : amount_sent_(0), allow_all_cookies_(true) {}
  void set_allow_all_cookies(bool allow_all_cookies) {
    allow_all_cookies_ = allow_all_cookies;
  }
  virtual ~MockSocketStreamDelegate() {}

  void SetOnStartOpenConnection(const base::Closure& callback) {
    on_start_open_connection_ = callback;
  }
  void SetOnConnected(const base::Closure& callback) {
    on_connected_ = callback;
  }
  void SetOnSentData(const base::Closure& callback) {
    on_sent_data_ = callback;
  }
  void SetOnReceivedData(const base::Closure& callback) {
    on_received_data_ = callback;
  }
  void SetOnClose(const base::Closure& callback) {
    on_close_ = callback;
  }

  virtual int OnStartOpenConnection(
      net::SocketStream* socket,
      const net::CompletionCallback& callback) OVERRIDE {
    if (!on_start_open_connection_.is_null())
      on_start_open_connection_.Run();
    return net::OK;
  }
  virtual void OnConnected(net::SocketStream* socket,
                           int max_pending_send_allowed) OVERRIDE {
    if (!on_connected_.is_null())
      on_connected_.Run();
  }
  virtual void OnSentData(net::SocketStream* socket,
                          int amount_sent) OVERRIDE {
    amount_sent_ += amount_sent;
    if (!on_sent_data_.is_null())
      on_sent_data_.Run();
  }
  virtual void OnReceivedData(net::SocketStream* socket,
                              const char* data, int len) OVERRIDE {
    received_data_ += std::string(data, len);
    if (!on_received_data_.is_null())
      on_received_data_.Run();
  }
  virtual void OnClose(net::SocketStream* socket) {
    if (!on_close_.is_null())
      on_close_.Run();
  }
  virtual bool CanGetCookies(net::SocketStream* socket,
                             const GURL& url) OVERRIDE {
    return allow_all_cookies_;
  }
  virtual bool CanSetCookie(net::SocketStream* request,
                            const GURL& url,
                            const std::string& cookie_line,
                            net::CookieOptions* options) OVERRIDE {
    return allow_all_cookies_;
  }

  size_t amount_sent() const { return amount_sent_; }
  const std::string& received_data() const { return received_data_; }

 private:
  int amount_sent_;
  bool allow_all_cookies_;
  std::string received_data_;
  base::Closure on_start_open_connection_;
  base::Closure on_connected_;
  base::Closure on_sent_data_;
  base::Closure on_received_data_;
  base::Closure on_close_;
};

class MockCookieStore : public net::CookieStore {
 public:
  struct Entry {
    GURL url;
    std::string cookie_line;
    net::CookieOptions options;
  };

  MockCookieStore() {}

  bool SetCookieWithOptions(const GURL& url,
                            const std::string& cookie_line,
                            const net::CookieOptions& options) {
    Entry entry;
    entry.url = url;
    entry.cookie_line = cookie_line;
    entry.options = options;
    entries_.push_back(entry);
    return true;
  }

  std::string GetCookiesWithOptions(const GURL& url,
                                    const net::CookieOptions& options) {
    std::string result;
    for (size_t i = 0; i < entries_.size(); i++) {
      Entry& entry = entries_[i];
      if (url == entry.url) {
        if (!result.empty()) {
          result += "; ";
        }
        result += entry.cookie_line;
      }
    }
    return result;
  }

  // CookieStore:
  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const net::CookieOptions& options,
      const SetCookiesCallback& callback) OVERRIDE {
    bool result = SetCookieWithOptions(url, cookie_line, options);
    if (!callback.is_null())
      callback.Run(result);
  }

  virtual void GetCookiesWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookiesCallback& callback) OVERRIDE {
    if (!callback.is_null())
      callback.Run(GetCookiesWithOptions(url, options));
  }

  virtual void GetCookiesWithInfoAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookieInfoCallback& callback) OVERRIDE {
    ADD_FAILURE();
  }

  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 const base::Closure& callback) OVERRIDE {
    ADD_FAILURE();
  }

  virtual void DeleteAllCreatedBetweenAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const DeleteCallback& callback) OVERRIDE {
    ADD_FAILURE();
  }

  virtual void DeleteSessionCookiesAsync(const DeleteCallback&) OVERRIDE {
    ADD_FAILURE();
  }

  virtual net::CookieMonster* GetCookieMonster() OVERRIDE { return NULL; }

  const std::vector<Entry>& entries() const { return entries_; }

 private:
  friend class base::RefCountedThreadSafe<MockCookieStore>;
  virtual ~MockCookieStore() {}

  std::vector<Entry> entries_;
};

class MockSSLConfigService : public net::SSLConfigService {
 public:
  virtual void GetSSLConfig(net::SSLConfig* config) OVERRIDE {}

 protected:
  virtual ~MockSSLConfigService() {}
};

class MockURLRequestContext : public net::URLRequestContext {
 public:
  explicit MockURLRequestContext(net::CookieStore* cookie_store)
      : transport_security_state_() {
    set_cookie_store(cookie_store);
    set_transport_security_state(&transport_security_state_);
    net::TransportSecurityState::DomainState state;
    state.upgrade_expiry = base::Time::Now() +
        base::TimeDelta::FromSeconds(1000);
    transport_security_state_.EnableHost("upgrademe.com", state);
  }

  virtual ~MockURLRequestContext() {}

 private:
  net::TransportSecurityState transport_security_state_;
};

class MockHttpTransactionFactory : public net::HttpTransactionFactory {
 public:
  explicit MockHttpTransactionFactory(net::OrderedSocketData* data) {
    data_ = data;
    net::MockConnect connect_data(net::SYNCHRONOUS, net::OK);
    data_->set_connect_data(connect_data);
    session_deps_.reset(new SpdySessionDependencies);
    session_deps_->socket_factory->AddSocketDataProvider(data_);
    http_session_ =
        SpdySessionDependencies::SpdyCreateSession(session_deps_.get());
    host_port_pair_.set_host("example.com");
    host_port_pair_.set_port(80);
    host_port_proxy_pair_.first = host_port_pair_;
    host_port_proxy_pair_.second = net::ProxyServer::Direct();
    net::SpdySessionPool* spdy_session_pool =
        http_session_->spdy_session_pool();
    DCHECK(spdy_session_pool);
    EXPECT_FALSE(spdy_session_pool->HasSession(host_port_proxy_pair_));
    session_ =
        spdy_session_pool->Get(host_port_proxy_pair_, net::BoundNetLog());
    EXPECT_TRUE(spdy_session_pool->HasSession(host_port_proxy_pair_));

    transport_params_ = new net::TransportSocketParams(host_port_pair_,
                                                       net::MEDIUM,
                                                       false,
                                                       false);
    net::ClientSocketHandle* connection = new net::ClientSocketHandle;
    EXPECT_EQ(net::OK,
              connection->Init(host_port_pair_.ToString(), transport_params_,
                               net::MEDIUM, net::CompletionCallback(),
                               http_session_->GetTransportSocketPool(
                                   net::HttpNetworkSession::NORMAL_SOCKET_POOL),
                               net::BoundNetLog()));
    EXPECT_EQ(net::OK,
              session_->InitializeWithSocket(connection, false, net::OK));
  }

  virtual int CreateTransaction(
      scoped_ptr<net::HttpTransaction>* trans) OVERRIDE {
    NOTREACHED();
    return net::ERR_UNEXPECTED;
  }

  virtual net::HttpCache* GetCache() OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  virtual net::HttpNetworkSession* GetSession() OVERRIDE {
    return http_session_.get();
  }

 private:
  net::OrderedSocketData* data_;
  scoped_ptr<SpdySessionDependencies> session_deps_;
  scoped_refptr<net::HttpNetworkSession> http_session_;
  scoped_refptr<net::TransportSocketParams> transport_params_;
  scoped_refptr<net::SpdySession> session_;
  net::HostPortPair host_port_pair_;
  net::HostPortProxyPair host_port_proxy_pair_;
};
}  // namespace

namespace net {

class WebSocketJobSpdy2Test : public PlatformTest {
 public:
  virtual void SetUp() OVERRIDE {
    SpdySession::set_default_protocol(kProtoSPDY2);
    stream_type_ = STREAM_INVALID;
    cookie_store_ = new MockCookieStore;
    context_.reset(new MockURLRequestContext(cookie_store_.get()));
  }
  virtual void TearDown() OVERRIDE {
    cookie_store_ = NULL;
    context_.reset();
    websocket_ = NULL;
    socket_ = NULL;
  }
  void DoSendRequest() {
    EXPECT_TRUE(websocket_->SendData(kHandshakeRequestWithoutCookie,
                                     kHandshakeRequestWithoutCookieLength));
  }
  void DoSendData() {
    if (received_data().size() == kHandshakeResponseWithoutCookieLength)
      websocket_->SendData(kDataHello, kDataHelloLength);
  }
  void DoSync() {
    sync_test_callback_.callback().Run(OK);
  }
  int WaitForResult() {
    return sync_test_callback_.WaitForResult();
  }
 protected:
  enum StreamType {
    STREAM_INVALID,
    STREAM_MOCK_SOCKET,
    STREAM_SOCKET,
    STREAM_SPDY_WEBSOCKET,
  };
  enum ThrottlingOption {
    THROTTLING_OFF,
    THROTTLING_ON,
  };
  enum SpdyOption {
    SPDY_OFF,
    SPDY_ON,
  };
  void InitWebSocketJob(const GURL& url,
                        MockSocketStreamDelegate* delegate,
                        StreamType stream_type) {
    DCHECK_NE(STREAM_INVALID, stream_type);
    stream_type_ = stream_type;
    websocket_ = new WebSocketJob(delegate);

    if (stream_type == STREAM_MOCK_SOCKET)
      socket_ = new MockSocketStream(url, websocket_.get());

    if (stream_type == STREAM_SOCKET || stream_type == STREAM_SPDY_WEBSOCKET) {
      if (stream_type == STREAM_SPDY_WEBSOCKET) {
        http_factory_.reset(new MockHttpTransactionFactory(data_.get()));
        context_->set_http_transaction_factory(http_factory_.get());
      }

      ssl_config_service_ = new MockSSLConfigService();
      context_->set_ssl_config_service(ssl_config_service_);
      proxy_service_.reset(net::ProxyService::CreateDirect());
      context_->set_proxy_service(proxy_service_.get());
      host_resolver_.reset(new net::MockHostResolver);
      context_->set_host_resolver(host_resolver_.get());

      socket_ = new SocketStream(url, websocket_.get());
      socket_factory_.reset(new MockClientSocketFactory);
      DCHECK(data_.get());
      socket_factory_->AddSocketDataProvider(data_.get());
      socket_->SetClientSocketFactory(socket_factory_.get());
    }

    websocket_->InitSocketStream(socket_.get());
    websocket_->set_context(context_.get());
    IPAddressNumber ip;
    ParseIPLiteralToNumber("127.0.0.1", &ip);
    websocket_->addresses_ = AddressList::CreateFromIPAddress(ip, 0);
  }
  void SkipToConnecting() {
    websocket_->state_ = WebSocketJob::CONNECTING;
    WebSocketThrottle::GetInstance()->PutInQueue(websocket_);
  }
  WebSocketJob::State GetWebSocketJobState() {
    return websocket_->state_;
  }
  void CloseWebSocketJob() {
    if (websocket_->socket_) {
      websocket_->socket_->DetachDelegate();
      WebSocketThrottle::GetInstance()->RemoveFromQueue(websocket_);
    }
    websocket_->state_ = WebSocketJob::CLOSED;
    websocket_->delegate_ = NULL;
    websocket_->socket_ = NULL;
  }
  SocketStream* GetSocket(SocketStreamJob* job) {
    return job->socket_.get();
  }
  const std::string& sent_data() const {
    DCHECK_EQ(STREAM_MOCK_SOCKET, stream_type_);
    MockSocketStream* socket =
        static_cast<MockSocketStream*>(socket_.get());
    DCHECK(socket);
    return socket->sent_data();
  }
  const std::string& received_data() const {
    DCHECK_NE(STREAM_INVALID, stream_type_);
    MockSocketStreamDelegate* delegate =
        static_cast<MockSocketStreamDelegate*>(websocket_->delegate_);
    DCHECK(delegate);
    return delegate->received_data();
  }

  void TestSimpleHandshake();
  void TestSlowHandshake();
  void TestHandshakeWithCookie();
  void TestHandshakeWithCookieButNotAllowed();
  void TestHSTSUpgrade();
  void TestInvalidSendData();
  void TestConnectByWebSocket(ThrottlingOption throttling);
  void TestConnectBySpdy(SpdyOption spdy, ThrottlingOption throttling);

  StreamType stream_type_;
  scoped_refptr<MockCookieStore> cookie_store_;
  scoped_ptr<MockURLRequestContext> context_;
  scoped_refptr<WebSocketJob> websocket_;
  scoped_refptr<SocketStream> socket_;
  scoped_ptr<MockClientSocketFactory> socket_factory_;
  scoped_ptr<OrderedSocketData> data_;
  TestCompletionCallback sync_test_callback_;
  scoped_refptr<MockSSLConfigService> ssl_config_service_;
  scoped_ptr<net::ProxyService> proxy_service_;
  scoped_ptr<net::MockHostResolver> host_resolver_;
  scoped_ptr<MockHttpTransactionFactory> http_factory_;

  static const char kHandshakeRequestWithoutCookie[];
  static const char kHandshakeRequestWithCookie[];
  static const char kHandshakeRequestWithFilteredCookie[];
  static const char kHandshakeResponseWithoutCookie[];
  static const char kHandshakeResponseWithCookie[];
  static const char kDataHello[];
  static const char kDataWorld[];
  static const char* const kHandshakeRequestForSpdy[];
  static const char* const kHandshakeResponseForSpdy[];
  static const size_t kHandshakeRequestWithoutCookieLength;
  static const size_t kHandshakeRequestWithCookieLength;
  static const size_t kHandshakeRequestWithFilteredCookieLength;
  static const size_t kHandshakeResponseWithoutCookieLength;
  static const size_t kHandshakeResponseWithCookieLength;
  static const size_t kDataHelloLength;
  static const size_t kDataWorldLength;

 private:
  SpdyTestStateHelper spdy_state_;
};

const char WebSocketJobSpdy2Test::kHandshakeRequestWithoutCookie[] =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Upgrade: WebSocket\r\n"
    "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
    "Origin: http://example.com\r\n"
    "\r\n"
    "^n:ds[4U";

const char WebSocketJobSpdy2Test::kHandshakeRequestWithCookie[] =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Upgrade: WebSocket\r\n"
    "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
    "Origin: http://example.com\r\n"
    "Cookie: WK-test=1\r\n"
    "\r\n"
    "^n:ds[4U";

const char WebSocketJobSpdy2Test::kHandshakeRequestWithFilteredCookie[] =
    "GET /demo HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key2: 12998 5 Y3 1  .P00\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Upgrade: WebSocket\r\n"
    "Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5\r\n"
    "Origin: http://example.com\r\n"
    "Cookie: CR-test=1; CR-test-httponly=1\r\n"
    "\r\n"
    "^n:ds[4U";

const char WebSocketJobSpdy2Test::kHandshakeResponseWithoutCookie[] =
    "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Origin: http://example.com\r\n"
    "Sec-WebSocket-Location: ws://example.com/demo\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "\r\n"
    "8jKS'y:G*Co,Wxa-";

const char WebSocketJobSpdy2Test::kHandshakeResponseWithCookie[] =
    "HTTP/1.1 101 WebSocket Protocol Handshake\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Origin: http://example.com\r\n"
    "Sec-WebSocket-Location: ws://example.com/demo\r\n"
    "Sec-WebSocket-Protocol: sample\r\n"
    "Set-Cookie: CR-set-test=1\r\n"
    "\r\n"
    "8jKS'y:G*Co,Wxa-";

const char WebSocketJobSpdy2Test::kDataHello[] = "Hello, ";

const char WebSocketJobSpdy2Test::kDataWorld[] = "World!\n";

// TODO(toyoshim): I should clarify which WebSocket headers for handshake must
// be exported to SPDY SYN_STREAM and SYN_REPLY.
// Because it depends on HyBi versions, just define it as follow for now.
const char* const WebSocketJobSpdy2Test::kHandshakeRequestForSpdy[] = {
  "host", "example.com",
  "origin", "http://example.com",
  "sec-websocket-protocol", "sample",
  "url", "ws://example.com/demo"
};

const char* const WebSocketJobSpdy2Test::kHandshakeResponseForSpdy[] = {
  "sec-websocket-origin", "http://example.com",
  "sec-websocket-location", "ws://example.com/demo",
  "sec-websocket-protocol", "sample",
};

const size_t WebSocketJobSpdy2Test::kHandshakeRequestWithoutCookieLength =
    arraysize(kHandshakeRequestWithoutCookie) - 1;
const size_t WebSocketJobSpdy2Test::kHandshakeRequestWithCookieLength =
    arraysize(kHandshakeRequestWithCookie) - 1;
const size_t WebSocketJobSpdy2Test::kHandshakeRequestWithFilteredCookieLength =
    arraysize(kHandshakeRequestWithFilteredCookie) - 1;
const size_t WebSocketJobSpdy2Test::kHandshakeResponseWithoutCookieLength =
    arraysize(kHandshakeResponseWithoutCookie) - 1;
const size_t WebSocketJobSpdy2Test::kHandshakeResponseWithCookieLength =
    arraysize(kHandshakeResponseWithCookie) - 1;
const size_t WebSocketJobSpdy2Test::kDataHelloLength =
    arraysize(kDataHello) - 1;
const size_t WebSocketJobSpdy2Test::kDataWorldLength =
    arraysize(kDataWorld) - 1;

void WebSocketJobSpdy2Test::TestSimpleHandshake() {
  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  DoSendRequest();
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestWithoutCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(),
                         kHandshakeRequestWithoutCookieLength);
  EXPECT_EQ(kHandshakeRequestWithoutCookieLength, delegate.amount_sent());

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseWithoutCookie,
                             kHandshakeResponseWithoutCookieLength);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());
  CloseWebSocketJob();
}

void WebSocketJobSpdy2Test::TestSlowHandshake() {
  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  DoSendRequest();
  // We assume request is sent in one data chunk (from WebKit)
  // We don't support streaming request.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestWithoutCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(),
                         kHandshakeRequestWithoutCookieLength);
  EXPECT_EQ(kHandshakeRequestWithoutCookieLength, delegate.amount_sent());

  std::vector<std::string> lines;
  base::SplitString(kHandshakeResponseWithoutCookie, '\n', &lines);
  for (size_t i = 0; i < lines.size() - 2; i++) {
    std::string line = lines[i] + "\r\n";
    SCOPED_TRACE("Line: " + line);
    websocket_->OnReceivedData(socket_,
                               line.c_str(),
                               line.size());
    MessageLoop::current()->RunAllPending();
    EXPECT_TRUE(delegate.received_data().empty());
    EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  }
  websocket_->OnReceivedData(socket_.get(), "\r\n", 2);
  MessageLoop::current()->RunAllPending();
  EXPECT_TRUE(delegate.received_data().empty());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnReceivedData(socket_.get(), "8jKS'y:G*Co,Wxa-", 16);
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());
  CloseWebSocketJob();
}

TEST_F(WebSocketJobSpdy2Test, DelayedCookies) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  GURL url("ws://example.com/demo");
  GURL cookieUrl("http://example.com/demo");
  CookieOptions cookie_options;
  scoped_refptr<DelayedCookieMonster> cookie_store = new DelayedCookieMonster();
  context_->set_cookie_store(cookie_store);
  cookie_store->SetCookieWithOptionsAsync(
      cookieUrl, "CR-test=1", cookie_options,
      net::CookieMonster::SetCookiesCallback());
  cookie_options.set_include_httponly();
  cookie_store->SetCookieWithOptionsAsync(
      cookieUrl, "CR-test-httponly=1", cookie_options,
      net::CookieMonster::SetCookiesCallback());

  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  bool sent = websocket_->SendData(kHandshakeRequestWithCookie,
                                   kHandshakeRequestWithCookieLength);
  EXPECT_TRUE(sent);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestWithFilteredCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_,
                         kHandshakeRequestWithFilteredCookieLength);
  EXPECT_EQ(kHandshakeRequestWithCookieLength,
            delegate.amount_sent());

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseWithCookie,
                             kHandshakeResponseWithCookieLength);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());

  CloseWebSocketJob();
}

void WebSocketJobSpdy2Test::TestHandshakeWithCookie() {
  GURL url("ws://example.com/demo");
  GURL cookieUrl("http://example.com/demo");
  CookieOptions cookie_options;
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test=1", cookie_options);
  cookie_options.set_include_httponly();
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test-httponly=1", cookie_options);

  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  bool sent = websocket_->SendData(kHandshakeRequestWithCookie,
                                   kHandshakeRequestWithCookieLength);
  EXPECT_TRUE(sent);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestWithFilteredCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_,
                         kHandshakeRequestWithFilteredCookieLength);
  EXPECT_EQ(kHandshakeRequestWithCookieLength,
            delegate.amount_sent());

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseWithCookie,
                             kHandshakeResponseWithCookieLength);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());

  EXPECT_EQ(3U, cookie_store_->entries().size());
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[0].url);
  EXPECT_EQ("CR-test=1", cookie_store_->entries()[0].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[1].url);
  EXPECT_EQ("CR-test-httponly=1", cookie_store_->entries()[1].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[2].url);
  EXPECT_EQ("CR-set-test=1", cookie_store_->entries()[2].cookie_line);

  CloseWebSocketJob();
}

void WebSocketJobSpdy2Test::TestHandshakeWithCookieButNotAllowed() {
  GURL url("ws://example.com/demo");
  GURL cookieUrl("http://example.com/demo");
  CookieOptions cookie_options;
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test=1", cookie_options);
  cookie_options.set_include_httponly();
  cookie_store_->SetCookieWithOptions(
      cookieUrl, "CR-test-httponly=1", cookie_options);

  MockSocketStreamDelegate delegate;
  delegate.set_allow_all_cookies(false);
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  bool sent = websocket_->SendData(kHandshakeRequestWithCookie,
                                   kHandshakeRequestWithCookieLength);
  EXPECT_TRUE(sent);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestWithoutCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_, kHandshakeRequestWithoutCookieLength);
  EXPECT_EQ(kHandshakeRequestWithCookieLength,
            delegate.amount_sent());

  websocket_->OnReceivedData(socket_.get(),
                             kHandshakeResponseWithCookie,
                             kHandshakeResponseWithCookieLength);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeResponseWithoutCookie, delegate.received_data());
  EXPECT_EQ(WebSocketJob::OPEN, GetWebSocketJobState());

  EXPECT_EQ(2U, cookie_store_->entries().size());
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[0].url);
  EXPECT_EQ("CR-test=1", cookie_store_->entries()[0].cookie_line);
  EXPECT_EQ(cookieUrl, cookie_store_->entries()[1].url);
  EXPECT_EQ("CR-test-httponly=1", cookie_store_->entries()[1].cookie_line);

  CloseWebSocketJob();
}

void WebSocketJobSpdy2Test::TestHSTSUpgrade() {
  GURL url("ws://upgrademe.com/");
  MockSocketStreamDelegate delegate;
  scoped_refptr<SocketStreamJob> job =
      SocketStreamJob::CreateSocketStreamJob(
          url, &delegate, context_->transport_security_state(),
          context_->ssl_config_service());
  EXPECT_TRUE(GetSocket(job.get())->is_secure());
  job->DetachDelegate();

  url = GURL("ws://donotupgrademe.com/");
  job = SocketStreamJob::CreateSocketStreamJob(
      url, &delegate, context_->transport_security_state(),
      context_->ssl_config_service());
  EXPECT_FALSE(GetSocket(job.get())->is_secure());
  job->DetachDelegate();
}

void WebSocketJobSpdy2Test::TestInvalidSendData() {
  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  InitWebSocketJob(url, &delegate, STREAM_MOCK_SOCKET);
  SkipToConnecting();

  DoSendRequest();
  // We assume request is sent in one data chunk (from WebKit)
  // We don't support streaming request.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kHandshakeRequestWithoutCookie, sent_data());
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  websocket_->OnSentData(socket_.get(),
                         kHandshakeRequestWithoutCookieLength);
  EXPECT_EQ(kHandshakeRequestWithoutCookieLength, delegate.amount_sent());

  // We could not send any data until connection is established.
  bool sent = websocket_->SendData(kHandshakeRequestWithoutCookie,
                                   kHandshakeRequestWithoutCookieLength);
  EXPECT_FALSE(sent);
  EXPECT_EQ(WebSocketJob::CONNECTING, GetWebSocketJobState());
  CloseWebSocketJob();
}

// Following tests verify cooperation between WebSocketJob and SocketStream.
// Other former tests use MockSocketStream as SocketStream, so we could not
// check SocketStream behavior.
// OrderedSocketData provide socket level verifiation by checking out-going
// packets in comparison with the MockWrite array and emulating in-coming
// packets with MockRead array.

void WebSocketJobSpdy2Test::TestConnectByWebSocket(
    ThrottlingOption throttling) {
  // This is a test for verifying cooperation between WebSocketJob and
  // SocketStream. If |throttling| was |THROTTLING_OFF|, it test basic
  // situation. If |throttling| was |THROTTLING_ON|, throttling limits the
  // latter connection.
  MockWrite writes[] = {
    MockWrite(ASYNC,
              kHandshakeRequestWithoutCookie,
              kHandshakeRequestWithoutCookieLength,
              1),
    MockWrite(ASYNC,
              kDataHello,
              kDataHelloLength,
              3)
  };
  MockRead reads[] = {
    MockRead(ASYNC,
             kHandshakeResponseWithoutCookie,
             kHandshakeResponseWithoutCookieLength,
             2),
    MockRead(ASYNC,
             kDataWorld,
             kDataWorldLength,
             4),
    MockRead(SYNCHRONOUS, 0, 5)  // EOF
  };
  data_.reset(new OrderedSocketData(
      reads, arraysize(reads), writes, arraysize(writes)));

  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  WebSocketJobSpdy2Test* test = this;
  if (throttling == THROTTLING_ON)
    delegate.SetOnStartOpenConnection(
        base::Bind(&WebSocketJobSpdy2Test::DoSync, base::Unretained(test)));
  delegate.SetOnConnected(
      base::Bind(&WebSocketJobSpdy2Test::DoSendRequest,
                 base::Unretained(test)));
  delegate.SetOnReceivedData(
      base::Bind(&WebSocketJobSpdy2Test::DoSendData, base::Unretained(test)));
  delegate.SetOnClose(
      base::Bind(&WebSocketJobSpdy2Test::DoSync, base::Unretained(test)));
  InitWebSocketJob(url, &delegate, STREAM_SOCKET);

  scoped_refptr<WebSocketJob> block_websocket;
  if (throttling == THROTTLING_ON) {
    // Create former WebSocket object which obstructs the latter one.
    block_websocket = new WebSocketJob(NULL);
    block_websocket->addresses_ = AddressList(websocket_->address_list());
    WebSocketThrottle::GetInstance()->PutInQueue(block_websocket.get());
  }

  websocket_->Connect();

  if (throttling == THROTTLING_ON) {
    EXPECT_EQ(OK, WaitForResult());
    EXPECT_TRUE(websocket_->IsWaiting());

    // Remove the former WebSocket object from throttling queue to unblock the
    // latter.
    WebSocketThrottle::GetInstance()->RemoveFromQueue(block_websocket);
    block_websocket->state_ = WebSocketJob::CLOSED;
    block_websocket = NULL;
    WebSocketThrottle::GetInstance()->WakeupSocketIfNecessary();
  }

  EXPECT_EQ(OK, WaitForResult());
  EXPECT_TRUE(data_->at_read_eof());
  EXPECT_TRUE(data_->at_write_eof());
  EXPECT_EQ(WebSocketJob::CLOSED, GetWebSocketJobState());
}

void WebSocketJobSpdy2Test::TestConnectBySpdy(
    SpdyOption spdy, ThrottlingOption throttling) {
  // This is a test for verifying cooperation between WebSocketJob and
  // SocketStream in the situation we have SPDY session to the server. If
  // |throttling| was |THROTTLING_ON|, throttling limits the latter connection.
  // If you enabled spdy, you should specify |spdy| as |SPDY_ON|. Expected
  // results depend on its configuration.
  MockWrite writes_websocket[] = {
    MockWrite(ASYNC,
              kHandshakeRequestWithoutCookie,
              kHandshakeRequestWithoutCookieLength,
              1),
    MockWrite(ASYNC,
              kDataHello,
              kDataHelloLength,
              3)
  };
  MockRead reads_websocket[] = {
    MockRead(ASYNC,
             kHandshakeResponseWithoutCookie,
             kHandshakeResponseWithoutCookieLength,
             2),
    MockRead(ASYNC,
             kDataWorld,
             kDataWorldLength,
             4),
    MockRead(SYNCHRONOUS, 0, 5)  // EOF
  };

  const SpdyStreamId kStreamId = 1;
  scoped_ptr<SpdyFrame> request_frame(
      ConstructSpdyWebSocketHandshakeRequestFrame(
          kHandshakeRequestForSpdy,
          arraysize(kHandshakeRequestForSpdy) / 2,
          kStreamId,
          MEDIUM));
  scoped_ptr<SpdyFrame> response_frame(
      ConstructSpdyWebSocketHandshakeResponseFrame(
          kHandshakeResponseForSpdy,
          arraysize(kHandshakeResponseForSpdy) / 2,
          kStreamId,
          MEDIUM));
  scoped_ptr<SpdyFrame> data_hello_frame(
      ConstructSpdyWebSocketDataFrame(
          kDataHello,
          kDataHelloLength,
          kStreamId,
          false));
  scoped_ptr<SpdyFrame> data_world_frame(
      ConstructSpdyWebSocketDataFrame(
          kDataWorld,
          kDataWorldLength,
          kStreamId,
          false));
  MockWrite writes_spdy[] = {
    CreateMockWrite(*request_frame.get(), 1),
    CreateMockWrite(*data_hello_frame.get(), 3),
  };
  MockRead reads_spdy[] = {
    CreateMockRead(*response_frame.get(), 2),
    CreateMockRead(*data_world_frame.get(), 4),
    MockRead(SYNCHRONOUS, 0, 5)  // EOF
  };

  if (spdy == SPDY_ON)
    data_.reset(new OrderedSocketData(
        reads_spdy, arraysize(reads_spdy),
        writes_spdy, arraysize(writes_spdy)));
  else
    data_.reset(new OrderedSocketData(
        reads_websocket, arraysize(reads_websocket),
        writes_websocket, arraysize(writes_websocket)));

  GURL url("ws://example.com/demo");
  MockSocketStreamDelegate delegate;
  WebSocketJobSpdy2Test* test = this;
  if (throttling == THROTTLING_ON)
    delegate.SetOnStartOpenConnection(
        base::Bind(&WebSocketJobSpdy2Test::DoSync, base::Unretained(test)));
  delegate.SetOnConnected(
      base::Bind(&WebSocketJobSpdy2Test::DoSendRequest,
                 base::Unretained(test)));
  delegate.SetOnReceivedData(
      base::Bind(&WebSocketJobSpdy2Test::DoSendData, base::Unretained(test)));
  delegate.SetOnClose(
      base::Bind(&WebSocketJobSpdy2Test::DoSync, base::Unretained(test)));
  InitWebSocketJob(url, &delegate, STREAM_SPDY_WEBSOCKET);

  scoped_refptr<WebSocketJob> block_websocket;
  if (throttling == THROTTLING_ON) {
    // Create former WebSocket object which obstructs the latter one.
    block_websocket = new WebSocketJob(NULL);
    block_websocket->addresses_ = AddressList(websocket_->address_list());
    WebSocketThrottle::GetInstance()->PutInQueue(block_websocket.get());
  }

  websocket_->Connect();

  if (throttling == THROTTLING_ON) {
    EXPECT_EQ(OK, WaitForResult());
    EXPECT_TRUE(websocket_->IsWaiting());

    // Remove the former WebSocket object from throttling queue to unblock the
    // latter.
    WebSocketThrottle::GetInstance()->RemoveFromQueue(block_websocket);
    block_websocket->state_ = WebSocketJob::CLOSED;
    block_websocket = NULL;
    WebSocketThrottle::GetInstance()->WakeupSocketIfNecessary();
  }

  EXPECT_EQ(OK, WaitForResult());
  EXPECT_TRUE(data_->at_read_eof());
  EXPECT_TRUE(data_->at_write_eof());
  EXPECT_EQ(WebSocketJob::CLOSED, GetWebSocketJobState());
}

// Execute tests in both spdy-disabled mode and spdy-enabled mode.
TEST_F(WebSocketJobSpdy2Test, SimpleHandshake) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestSimpleHandshake();
}

TEST_F(WebSocketJobSpdy2Test, SlowHandshake) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestSlowHandshake();
}

TEST_F(WebSocketJobSpdy2Test, HandshakeWithCookie) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestHandshakeWithCookie();
}

TEST_F(WebSocketJobSpdy2Test, HandshakeWithCookieButNotAllowed) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestHandshakeWithCookieButNotAllowed();
}

TEST_F(WebSocketJobSpdy2Test, HSTSUpgrade) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestHSTSUpgrade();
}

TEST_F(WebSocketJobSpdy2Test, InvalidSendData) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestInvalidSendData();
}

TEST_F(WebSocketJobSpdy2Test, SimpleHandshakeSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestSimpleHandshake();
}

TEST_F(WebSocketJobSpdy2Test, SlowHandshakeSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestSlowHandshake();
}

TEST_F(WebSocketJobSpdy2Test, HandshakeWithCookieSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestHandshakeWithCookie();
}

TEST_F(WebSocketJobSpdy2Test, HandshakeWithCookieButNotAllowedSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestHandshakeWithCookieButNotAllowed();
}

TEST_F(WebSocketJobSpdy2Test, HSTSUpgradeSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestHSTSUpgrade();
}

TEST_F(WebSocketJobSpdy2Test, InvalidSendDataSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestInvalidSendData();
}

TEST_F(WebSocketJobSpdy2Test, ConnectByWebSocket) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestConnectByWebSocket(THROTTLING_OFF);
}

TEST_F(WebSocketJobSpdy2Test, ConnectByWebSocketSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestConnectByWebSocket(THROTTLING_OFF);
}

TEST_F(WebSocketJobSpdy2Test, ConnectBySpdy) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestConnectBySpdy(SPDY_OFF, THROTTLING_OFF);
}

TEST_F(WebSocketJobSpdy2Test, ConnectBySpdySpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestConnectBySpdy(SPDY_ON, THROTTLING_OFF);
}

TEST_F(WebSocketJobSpdy2Test, ThrottlingWebSocket) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestConnectByWebSocket(THROTTLING_ON);
}

TEST_F(WebSocketJobSpdy2Test, ThrottlingWebSocketSpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestConnectByWebSocket(THROTTLING_ON);
}

TEST_F(WebSocketJobSpdy2Test, ThrottlingSpdy) {
  WebSocketJob::set_websocket_over_spdy_enabled(false);
  TestConnectBySpdy(SPDY_OFF, THROTTLING_ON);
}

TEST_F(WebSocketJobSpdy2Test, ThrottlingSpdySpdyEnabled) {
  WebSocketJob::set_websocket_over_spdy_enabled(true);
  TestConnectBySpdy(SPDY_ON, THROTTLING_ON);
}

// TODO(toyoshim): Add tests to verify throttling, SPDY stream limitation.
// TODO(toyoshim,yutak): Add tests to verify closing handshake.

}  // namespace net
