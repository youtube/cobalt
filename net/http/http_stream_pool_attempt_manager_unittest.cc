// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_attempt_manager.h"

#include <array>
#include <list>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/port_util.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/alternative_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory_test_util.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_group.h"
#include "net/http/http_stream_pool_handle.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_context.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/socket/stream_socket_handle.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/ssl_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using ::testing::_;
using ::testing::Optional;

namespace net {

using test::IsError;
using test::IsOk;
using test::MockQuicData;
using test::QuicTestPacketMaker;

using Group = HttpStreamPool::Group;
using AttemptManager = HttpStreamPool::AttemptManager;
using Job = HttpStreamPool::Job;
using IPEndPointStateTracker = HttpStreamPool::IPEndPointStateTracker;

namespace {

constexpr std::string_view kDefaultServerName = "www.example.org";
constexpr std::string_view kDefaultDestination = "https://www.example.org";

IPEndPoint MakeIPEndPoint(std::string_view addr, uint16_t port = 80) {
  return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
}

void ValidateConnectTiming(LoadTimingInfo::ConnectTiming& connect_timing) {
  EXPECT_LE(connect_timing.domain_lookup_start,
            connect_timing.domain_lookup_end);
  EXPECT_LE(connect_timing.domain_lookup_end, connect_timing.connect_start);
  EXPECT_LE(connect_timing.connect_start, connect_timing.ssl_start);
  EXPECT_LE(connect_timing.ssl_start, connect_timing.ssl_end);
  // connectEnd should cover TLS handshake.
  EXPECT_LE(connect_timing.ssl_end, connect_timing.connect_end);
}

class Preconnector {
 public:
  explicit Preconnector(std::string_view destination) {
    key_builder_.set_destination(destination);
  }

  explicit Preconnector(const HttpStreamKey& key) {
    key_builder_.from_key(key);
  }

  Preconnector(const Preconnector&) = delete;
  Preconnector& operator=(const Preconnector&) = delete;

  ~Preconnector() = default;

  Preconnector& set_num_streams(size_t num_streams) {
    num_streams_ = num_streams;
    return *this;
  }

  Preconnector& set_quic_version(quic::ParsedQuicVersion quic_version) {
    AlternativeService alternative_service(NextProto::kProtoQUIC,
                                           key_builder_.destination().host(),
                                           key_builder_.destination().port());
    alternative_service_info_.set_alternative_service(alternative_service);
    alternative_service_info_.SetAdvertisedVersions({quic_version});
    return *this;
  }

  HttpStreamKey GetStreamKey() const { return key_builder_.Build(); }

  int Preconnect(HttpStreamPool& pool) {
    const HttpStreamKey stream_key = GetStreamKey();
    int rv = pool.Preconnect(
        HttpStreamPoolRequestInfo(
            stream_key.destination(), stream_key.privacy_mode(),
            stream_key.socket_tag(), stream_key.network_anonymization_key(),
            stream_key.secure_dns_policy(),
            stream_key.disable_cert_network_fetches(),
            alternative_service_info_, is_http1_allowed_, load_flags_,
            proxy_info_,
            NetLogWithSource::Make(
                pool.http_network_session()->net_log(),
                NetLogSourceType::HTTP_STREAM_JOB_CONTROLLER)),
        num_streams_,
        base::BindOnce(&Preconnector::OnComplete, base::Unretained(this)));
    if (rv != ERR_IO_PENDING) {
      result_ = rv;
    }
    return rv;
  }

  int WaitForResult() {
    if (result_.has_value()) {
      return *result_;
    }
    base::RunLoop run_loop;
    wait_result_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    CHECK(result_.has_value());
    return *result_;
  }

  std::optional<int> result() const { return result_; }

 private:
  void OnComplete(int rv) {
    result_ = rv;
    if (wait_result_closure_) {
      std::move(wait_result_closure_).Run();
    }
  }

  StreamKeyBuilder key_builder_;

  size_t num_streams_ = 1;

  AlternativeServiceInfo alternative_service_info_;
  bool is_http1_allowed_ = true;
  ProxyInfo proxy_info_ = ProxyInfo::Direct();
  int load_flags_ = 0;

  std::optional<int> result_;
  base::OnceClosure wait_result_closure_;
};

// A helper to request an HttpStream. On success, it keeps the provided
// HttpStream. On failure, it keeps error information.
class StreamRequester : public HttpStreamRequest::Delegate {
 public:
  StreamRequester() = default;

  explicit StreamRequester(const HttpStreamKey& key) {
    key_builder_.from_key(key);
  }

  StreamRequester(const StreamRequester&) = delete;
  StreamRequester& operator=(const StreamRequester&) = delete;

  ~StreamRequester() override = default;

  StreamRequester& set_destination(std::string_view destination) {
    key_builder_.set_destination(destination);
    return *this;
  }

  StreamRequester& set_destination(url::SchemeHostPort destination) {
    key_builder_.set_destination(destination);
    return *this;
  }

  StreamRequester& set_priority(RequestPriority priority) {
    priority_ = priority;
    return *this;
  }

  StreamRequester& set_enable_ip_based_pooling(bool enable_ip_based_pooling) {
    enable_ip_based_pooling_ = enable_ip_based_pooling;
    return *this;
  }

  StreamRequester& set_enable_alternative_services(
      bool enable_alternative_services) {
    enable_alternative_services_ = enable_alternative_services;
    return *this;
  }

  StreamRequester& set_is_http1_allowed(bool is_http1_allowed) {
    is_http1_allowed_ = is_http1_allowed;
    return *this;
  }

  StreamRequester& set_load_flags(int load_flags) {
    load_flags_ = load_flags;
    return *this;
  }

  StreamRequester& set_proxy_info(ProxyInfo proxy_info) {
    proxy_info_ = std::move(proxy_info);
    return *this;
  }

  StreamRequester& set_privacy_mode(PrivacyMode privacy_mode) {
    key_builder_.set_privacy_mode(privacy_mode);
    return *this;
  }

  StreamRequester& set_alternative_service_info(
      AlternativeServiceInfo alternative_service_info) {
    alternative_service_info_ = std::move(alternative_service_info);
    return *this;
  }

  StreamRequester& set_quic_version(quic::ParsedQuicVersion quic_version) {
    AlternativeService alternative_service(NextProto::kProtoQUIC,
                                           key_builder_.destination().host(),
                                           key_builder_.destination().port());
    alternative_service_info_.set_alternative_service(alternative_service);
    alternative_service_info_.SetAdvertisedVersions({quic_version});
    return *this;
  }

  HttpStreamKey GetStreamKey() const { return key_builder_.Build(); }

  HttpStreamRequest* RequestStream(HttpStreamPool& pool) {
    const HttpStreamKey stream_key = GetStreamKey();
    request_ = pool.RequestStream(
        this,
        HttpStreamPoolRequestInfo(
            stream_key.destination(), stream_key.privacy_mode(),
            stream_key.socket_tag(), stream_key.network_anonymization_key(),
            stream_key.secure_dns_policy(),
            stream_key.disable_cert_network_fetches(),
            alternative_service_info_, is_http1_allowed_, load_flags_,
            proxy_info_,
            NetLogWithSource::Make(
                pool.http_network_session()->net_log(),
                NetLogSourceType::HTTP_STREAM_JOB_CONTROLLER)),
        priority_, allowed_bad_certs_, enable_ip_based_pooling_,
        enable_alternative_services_,
        NetLogWithSource::Make(pool.http_network_session()->net_log(),
                               NetLogSourceType::URL_REQUEST));
    Group* group = pool.GetGroupForTesting(stream_key);
    AttemptManager* attempt_manager =
        group ? group->attempt_manager() : nullptr;
    if (attempt_manager) {
      associated_attempt_manager_ = attempt_manager->GetWeakPtrForTesting();
    }
    return request_.get();
  }

  int WaitForResult() {
    if (result_.has_value()) {
      return *result_;
    }
    base::RunLoop run_loop;
    wait_result_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    CHECK(result_.has_value());
    return *result_;
  }

  void ResetRequest() { request_.reset(); }

  // HttpStreamRequest::Delegate methods:
  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override {
    used_proxy_info_ = used_proxy_info;
    stream_ = std::move(stream);
    SetResult(OK);
  }

  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override {
    NOTREACHED();
  }

  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override {
    NOTREACHED();
  }

  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_error_info) override {
    net_error_details_ = net_error_details;
    used_proxy_info_ = used_proxy_info;
    resolve_error_info_ = resolve_error_info;
    SetResult(status);
  }

  void OnCertificateError(int status, const SSLInfo& ssl_info) override {
    cert_error_ssl_info_ = ssl_info;
    SetResult(status);
  }

  void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override {
    NOTREACHED();
  }

  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override {
    CHECK(!cert_info_);
    cert_info_ = cert_info;
    SetResult(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  }

  void OnQuicBroken() override {}

  void OnSwitchesToHttpStreamPool(
      HttpStreamPoolRequestInfo request_info) override {}

  HttpStream* stream() {
    CHECK(stream_);
    return stream_.get();
  }

  std::unique_ptr<HttpStream> ReleaseStream() { return std::move(stream_); }

  std::optional<int> result() const { return result_; }

  const NetErrorDetails& net_error_details() const {
    return net_error_details_;
  }

  const ResolveErrorInfo& resolve_error_info() const {
    return resolve_error_info_;
  }

  const SSLInfo& cert_error_ssl_info() const { return cert_error_ssl_info_; }

  scoped_refptr<SSLCertRequestInfo> cert_info() const { return cert_info_; }

  NextProto negotiated_protocol() const {
    return request_->negotiated_protocol();
  }

  const ConnectionAttempts& connection_attempts() const {
    return request_->connection_attempts();
  }

  const ProxyInfo& used_proxy_info() const { return used_proxy_info_; }

  base::WeakPtr<AttemptManager> associated_attempt_manager() {
    return associated_attempt_manager_;
  }

 private:
  void SetResult(int rv) {
    result_ = rv;
    if (wait_result_closure_) {
      std::move(wait_result_closure_).Run();
    }
  }

  StreamKeyBuilder key_builder_;

  RequestPriority priority_ = RequestPriority::IDLE;

  std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;

  bool enable_ip_based_pooling_ = true;
  bool enable_alternative_services_ = true;
  bool is_http1_allowed_ = true;
  int load_flags_ = 0;
  ProxyInfo proxy_info_ = ProxyInfo::Direct();
  AlternativeServiceInfo alternative_service_info_;

  std::unique_ptr<HttpStreamRequest> request_;

  base::WeakPtr<AttemptManager> associated_attempt_manager_;

  base::OnceClosure wait_result_closure_;

  std::unique_ptr<HttpStream> stream_;
  std::optional<int> result_;
  NetErrorDetails net_error_details_;
  ResolveErrorInfo resolve_error_info_;
  SSLInfo cert_error_ssl_info_;
  scoped_refptr<SSLCertRequestInfo> cert_info_;
  ProxyInfo used_proxy_info_;
};

}  // namespace

class HttpStreamPoolAttemptManagerTest : public TestWithTaskEnvironment {
 public:
  HttpStreamPoolAttemptManagerTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    feature_list_.InitAndEnableFeature(features::kHappyEyeballsV3);
    InitializeSession();
  }

 protected:
  void InitializeSession() {
    http_network_session_.reset();
    session_deps_.alternate_host_resolver =
        std::make_unique<FakeServiceEndpointResolver>();

    auto quic_context = std::make_unique<MockQuicContext>();
    quic_context->AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(20));
    quic_context->params()->origins_to_force_quic_on =
        origins_to_force_quic_on_;
    session_deps_.quic_context = std::move(quic_context);
    session_deps_.enable_quic = true;

    // Load a certificate that is valid for *.example.org
    scoped_refptr<X509Certificate> test_cert(
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem"));
    EXPECT_TRUE(test_cert.get());
    verify_details_.cert_verify_result.verified_cert = test_cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    auto mock_crypto_client_stream_factory =
        std::make_unique<MockCryptoClientStreamFactory>();
    mock_crypto_client_stream_factory->AddProofVerifyDetails(&verify_details_);
    mock_crypto_client_stream_factory->set_handshake_mode(
        MockCryptoClientStream::CONFIRM_HANDSHAKE);
    session_deps_.quic_crypto_client_stream_factory =
        std::move(mock_crypto_client_stream_factory);

    SSLContextConfig config;
    config.ech_enabled = true;
    session_deps_.ssl_config_service =
        std::make_unique<TestSSLConfigService>(config);

    http_network_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  }

  void DestroyHttpNetworkSession() { http_network_session_.reset(); }

  void SetEchEnabled(bool ech_enabled) {
    SSLContextConfig config = ssl_config_service()->GetSSLContextConfig();
    config.ech_enabled = ech_enabled;
    ssl_config_service()->UpdateSSLConfigAndNotify(config);
  }

  HttpStreamPool& pool() { return *http_network_session_->http_stream_pool(); }

  FakeServiceEndpointResolver* resolver() {
    return static_cast<FakeServiceEndpointResolver*>(
        session_deps_.alternate_host_resolver.get());
  }

  MockClientSocketFactory* socket_factory() {
    return session_deps_.socket_factory.get();
  }

  TestSSLConfigService* ssl_config_service() {
    return static_cast<TestSSLConfigService*>(
        session_deps_.ssl_config_service.get());
  }

  MockCryptoClientStreamFactory* crypto_client_stream_factory() {
    return static_cast<MockCryptoClientStreamFactory*>(
        session_deps_.quic_crypto_client_stream_factory.get());
  }

  HttpNetworkSession* http_network_session() {
    return http_network_session_.get();
  }

  HttpServerProperties* http_server_properties() {
    return http_network_session_->http_server_properties();
  }

  SpdySessionPool* spdy_session_pool() {
    return http_network_session_->spdy_session_pool();
  }

  QuicSessionPool* quic_session_pool() {
    return http_network_session_->quic_session_pool();
  }

  quic::ParsedQuicVersion quic_version() {
    return quic::ParsedQuicVersion::RFCv1();
  }

  base::WeakPtr<SpdySession> CreateFakeSpdySession(
      const HttpStreamKey& stream_key,
      IPEndPoint peer_addr = IPEndPoint(IPAddress(192, 0, 2, 1), 443)) {
    Group& group = pool().GetOrCreateGroupForTesting(stream_key);
    CHECK(!spdy_session_pool()->HasAvailableSession(
        group.spdy_session_key(),
        /*enable_ip_based_pooling=*/true,
        /*is_websocket=*/false));
    auto socket = FakeStreamSocket::CreateForSpdy();
    socket->set_peer_addr(peer_addr);
    auto handle = group.CreateHandle(
        std::move(socket), StreamSocketHandle::SocketReuseType::kUnused,
        LoadTimingInfo::ConnectTiming());

    base::WeakPtr<SpdySession> spdy_session;
    int rv = spdy_session_pool()->CreateAvailableSessionFromSocketHandle(
        group.spdy_session_key(), std::move(handle), NetLogWithSource(),
        MultiplexedSessionCreationInitiator::kUnknown, &spdy_session);
    CHECK_EQ(rv, OK);
    // See the comment of CreateFakeSpdySession() in spdy_test_util_common.cc.
    spdy_session->SetTimeToBufferSmallWindowUpdates(base::TimeDelta::Max());
    return spdy_session;
  }

  void AddQuicData(
      std::string_view host = kDefaultServerName,
      MockConnectCompleter* connect_completer = nullptr,
      quic::QuicErrorCode close_error = quic::QUIC_CONNECTION_CANCELLED,
      std::string_view close_error_details = "net error") {
    auto client_maker = std::make_unique<QuicTestPacketMaker>(
        quic_version(),
        quic::QuicUtils::CreateRandomConnectionId(
            session_deps_.quic_context->random_generator()),
        session_deps_.quic_context->clock(), std::string(host),
        quic::Perspective::IS_CLIENT);

    auto quic_data = std::make_unique<MockQuicData>(quic_version());

    int packet_number = 1;
    quic_data->AddReadPauseForever();
    if (connect_completer) {
      quic_data->AddConnect(connect_completer);
    } else {
      quic_data->AddConnect(ASYNC, OK);
    }
    // HTTP/3 SETTINGS are always the first thing sent on a connection.
    quic_data->AddWrite(SYNCHRONOUS, client_maker->MakeInitialSettingsPacket(
                                         /*packet_number=*/packet_number++));
    // Connection close on shutdown.
    quic_data->AddWrite(SYNCHRONOUS,
                        client_maker->Packet(packet_number++)
                            .AddConnectionCloseFrame(
                                close_error, std::string(close_error_details),
                                quic::NO_IETF_QUIC_ERROR)
                            .Build());
    quic_data->AddSocketDataToFactory(socket_factory());

    quic_client_makers_.emplace_back(std::move(client_maker));
    mock_quic_datas_.emplace_back(std::move(quic_data));
  }

  QuicTestPacketMaker* CreateQuicClientPacketMaker(
      std::string_view host = kDefaultServerName) {
    auto client_maker = std::make_unique<QuicTestPacketMaker>(
        quic_version(),
        quic::QuicUtils::CreateRandomConnectionId(
            session_deps_.quic_context->random_generator()),
        session_deps_.quic_context->clock(), std::string(host),
        quic::Perspective::IS_CLIENT);
    QuicTestPacketMaker* raw_client_maker = client_maker.get();
    quic_client_makers_.emplace_back(std::move(client_maker));
    return raw_client_maker;
  }

  std::set<HostPortPair>& origins_to_force_quic_on() {
    return origins_to_force_quic_on_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // For NetLog recording test coverage.
  RecordingNetLogObserver net_log_observer_;

  SpdySessionDependencies session_deps_;

  std::set<HostPortPair> origins_to_force_quic_on_;

  ProofVerifyDetailsChromium verify_details_;
  std::vector<std::unique_ptr<QuicTestPacketMaker>> quic_client_makers_;
  std::vector<std::unique_ptr<MockQuicData>> mock_quic_datas_;

  std::unique_ptr<HttpNetworkSession> http_network_session_;
};

TEST_F(HttpStreamPoolAttemptManagerTest, ResolveEndpointFailedSync) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request->CompleteStartSynchronously(ERR_FAILED);
  StreamRequester requester;
  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));

  // Resetting the request should release the corresponding job(s).
  requester.ResetRequest();
  EXPECT_EQ(pool().JobControllerCountForTesting(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ResolveEndpointFailedMultipleRequests) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.RequestStream(pool());

  StreamRequester requester2;
  requester2.RequestStream(pool());

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  RunUntilIdle();

  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_FAILED)));
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_FAILED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, LoadState) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());

  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_FAILED);
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_IDLE);
}

TEST_F(HttpStreamPoolAttemptManagerTest, ResolveErrorInfo) {
  ResolveErrorInfo resolve_error_info(ERR_NAME_NOT_RESOLVED);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request->set_resolve_error_info(resolve_error_info);

  StreamRequester requester;
  requester.RequestStream(pool());

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_NAME_NOT_RESOLVED)));
  ASSERT_EQ(requester.resolve_error_info(), resolve_error_info);
  ASSERT_EQ(requester.connection_attempts().size(), 1u);
  EXPECT_EQ(requester.connection_attempts()[0].result, ERR_NAME_NOT_RESOLVED);
}

TEST_F(HttpStreamPoolAttemptManagerTest, DnsAliases) {
  const std::set<std::string> kAliases = {"alias1", "alias2"};
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_aliases(kAliases)
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  EXPECT_THAT(stream->GetDnsAliases(), kAliases);
}

TEST_F(HttpStreamPoolAttemptManagerTest, ConnectTiming) {
  constexpr base::TimeDelta kDnsUpdateDelay = base::Milliseconds(20);
  constexpr base::TimeDelta kDnsFinishDelay = base::Milliseconds(10);
  constexpr base::TimeDelta kTcpDelay = base::Milliseconds(20);
  constexpr base::TimeDelta kTlsDelay = base::Milliseconds(90);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  MockConnectCompleter tcp_connect_completer;
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(&tcp_connect_completer));
  socket_factory()->AddSocketDataProvider(data.get());

  MockConnectCompleter tls_connect_completer;
  auto ssl = std::make_unique<SSLSocketDataProvider>(&tls_connect_completer);
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  FastForwardBy(kDnsUpdateDelay);
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(false)
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(kDnsFinishDelay);
  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointRequestFinished(
      OK);
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(kTcpDelay);
  tcp_connect_completer.Complete(OK);
  RunUntilIdle();
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(kTlsDelay);
  tls_connect_completer.Complete(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();

  // Initialize `stream` to make load timing info available.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  stream->InitializeStream(/*can_send_early=*/false, RequestPriority::IDLE,
                           NetLogWithSource(), base::DoNothing());

  LoadTimingInfo timing_info;
  ASSERT_TRUE(stream->GetLoadTimingInfo(&timing_info));

  LoadTimingInfo::ConnectTiming& connect_timing = timing_info.connect_timing;

  ValidateConnectTiming(connect_timing);

  ASSERT_EQ(
      connect_timing.domain_lookup_end - connect_timing.domain_lookup_start,
      kDnsUpdateDelay);
  ASSERT_EQ(connect_timing.connect_end - connect_timing.connect_start,
            kDnsFinishDelay + kTcpDelay + kTlsDelay);
  ASSERT_EQ(connect_timing.ssl_end - connect_timing.ssl_start, kTlsDelay);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ConnectTimingDnsResolutionNotFinished) {
  constexpr base::TimeDelta kDnsUpdateDelay = base::Milliseconds(30);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("http://a.test").RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());

  FastForwardBy(kDnsUpdateDelay);
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  FastForwardBy(kDnsUpdateDelay);
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();

  // Initialize `stream` to make load timing info available.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  stream->InitializeStream(/*can_send_early=*/false, RequestPriority::IDLE,
                           NetLogWithSource(), base::DoNothing());

  LoadTimingInfo timing_info;
  ASSERT_TRUE(stream->GetLoadTimingInfo(&timing_info));
  ASSERT_EQ(timing_info.connect_timing.domain_lookup_end,
            timing_info.connect_timing.connect_start);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PlainHttpWaitForHttpsRecord) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("http://a.test").RequestStream(pool());

  // Notify there is a resolved IP address. The request should not make any
  // progress since it needs to wait for HTTPS RR.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);

  // Simulate triggering HTTP -> HTTPS upgrade.
  endpoint_request->CallOnServiceEndpointRequestFinished(
      ERR_DNS_NAME_HTTPS_ONLY);
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_DNS_NAME_HTTPS_ONLY)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SetPriority) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester1;
  HttpStreamRequest* request1 =
      requester1.set_priority(RequestPriority::LOW).RequestStream(pool());
  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester1.GetStreamKey())
          .attempt_manager();
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);
  ASSERT_EQ(manager->GetPriority(), RequestPriority::LOW);

  // Create another request with IDLE priority, which has lower than LOW.
  StreamRequester requester2;
  HttpStreamRequest* request2 =
      requester2.set_priority(RequestPriority::IDLE).RequestStream(pool());
  ASSERT_EQ(manager, pool()
                         .GetOrCreateGroupForTesting(requester2.GetStreamKey())
                         .attempt_manager());
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::LOW);
  ASSERT_EQ(manager->GetPriority(), RequestPriority::LOW);

  // Set the second request's priority to HIGHEST. The corresponding service
  // endpoint request and attempt manager should update their priorities.
  request2->SetPriority(RequestPriority::HIGHEST);
  ASSERT_EQ(endpoint_request->priority(), RequestPriority::HIGHEST);
  ASSERT_EQ(manager->GetPriority(), RequestPriority::HIGHEST);

  // Check `request2` completes first.

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data1.get());

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);
  ASSERT_EQ(request1->GetLoadState(), LOAD_STATE_CONNECTING);
  ASSERT_EQ(request2->GetLoadState(), LOAD_STATE_CONNECTING);

  RunUntilIdle();
  ASSERT_FALSE(request1->completed());
  ASSERT_TRUE(request2->completed());
  ASSERT_EQ(request1->GetLoadState(), LOAD_STATE_CONNECTING);
  ASSERT_EQ(request2->GetLoadState(), LOAD_STATE_IDLE);
  std::unique_ptr<HttpStream> stream = requester2.ReleaseStream();
  ASSERT_TRUE(stream);
}

// Regression test for crbug.com/397403548.
// AttemptManager should return a valid priority even after Job (request) is
// notified.
TEST_F(HttpStreamPoolAttemptManagerTest, GetPriority) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  MockConnectCompleter completer;
  data.set_connect_data(MockConnect(&completer));
  socket_factory()->AddSocketDataProvider(&data);

  // Request a stream with the HIGHEST priority.
  StreamRequester requester;
  requester.set_priority(RequestPriority::HIGHEST).RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Ensure the corresponding attempt manager has the HIGHEST priority.
  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .attempt_manager();
  ASSERT_EQ(manager->GetPriority(), RequestPriority::HIGHEST);

  // Complete the stream attempt and wait for request completion. The
  // corresponding attempt manager should return IDLE priority.
  completer.Complete(OK);
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  ASSERT_EQ(manager->GetPriority(), RequestPriority::IDLE);
}

TEST_F(HttpStreamPoolAttemptManagerTest, TcpFailSync) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(requester.connection_attempts().size(), 1u);
  ASSERT_EQ(requester.connection_attempts()[0].result, ERR_FAILED);
}

TEST_F(HttpStreamPoolAttemptManagerTest, TcpFailAsync) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(requester.connection_attempts().size(), 1u);
  ASSERT_EQ(requester.connection_attempts()[0].result, ERR_FAILED);
}

TEST_F(HttpStreamPoolAttemptManagerTest, TlsOkAsync) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, TcpSyncTlsAsyncOk) {
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, TlsCryptoReadyDelayed) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  HttpStreamRequest* request =
      requester.set_destination("https://a.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  ASSERT_FALSE(requester.result().has_value());
  ASSERT_EQ(request->GetLoadState(), LOAD_STATE_SSL_HANDSHAKE);

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, CertificateError) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const scoped_refptr<X509Certificate> kCert =
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_DATE_INVALID);
  ssl.ssl_info.cert_status = ERR_CERT_DATE_INVALID;
  ssl.ssl_info.cert = kCert;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  constexpr std::string_view kDestination = "https://a.test";
  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_FALSE(requester1.result().has_value());
  EXPECT_FALSE(requester2.result().has_value());

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_CERT_DATE_INVALID)));
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_CERT_DATE_INVALID)));
  ASSERT_TRUE(
      requester1.cert_error_ssl_info().cert->EqualsIncludingChain(kCert.get()));
  ASSERT_EQ(requester1.connection_attempts().size(), 1u);
  ASSERT_EQ(requester1.connection_attempts()[0].result, ERR_CERT_DATE_INVALID);

  ASSERT_TRUE(
      requester2.cert_error_ssl_info().cert->EqualsIncludingChain(kCert.get()));
  ASSERT_EQ(requester2.connection_attempts().size(), 1u);
  ASSERT_EQ(requester2.connection_attempts()[0].result, ERR_CERT_DATE_INVALID);
}

TEST_F(HttpStreamPoolAttemptManagerTest, NeedsClientAuth) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_FALSE(requester1.result().has_value());
  EXPECT_FALSE(requester2.result().has_value());

  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_EQ(requester1.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
  EXPECT_EQ(requester2.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
}

// Tests that after a fatal error (e.g., the server required a client cert),
// following attempt failures are ignored and the existing requests get the
// same fatal error.
TEST_F(HttpStreamPoolAttemptManagerTest, TcpFailAfterNeedsClientAuth) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data1 = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data1.get());
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data2.get());

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_EQ(requester1.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
  EXPECT_EQ(requester2.cert_info()->host_and_port,
            HostPortPair::FromSchemeHostPort(kDestination));
}

TEST_F(HttpStreamPoolAttemptManagerTest, RequestCanceledBeforeAttemptSuccess) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  requester.ResetRequest();
  RunUntilIdle();

  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

// Tests that canceling a limit ignoring request doesn't result in hitting a
// CHECK. Ensures that a group is destroyed after the attempt failed.
TEST_F(HttpStreamPoolAttemptManagerTest, LimitIgnoringRequestCanceled) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_load_flags(LOAD_IGNORE_LIMITS).RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  requester.ResetRequest();
  requester.ReleaseStream().reset();
  FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(pool().GetGroupForTesting(requester.GetStreamKey()));
}

// This test simulates a situation where:
// * AttemptManager has jobs (requests) more than the limit.
// * An attempt fails with a cert error.
// * QuicAttempt fails immediately after the attempt failed.
// Ensures that we don't attempt any further connections.
TEST_F(HttpStreamPoolAttemptManagerTest, DoNotAttemptWhileFailing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // Prepare QUIC data.
  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  // Create requests and mock connects more than the group limit.
  std::queue<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> data_providers;
  std::queue<std::unique_ptr<MockConnectCompleter>> ssl_completers;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssl_providers;
  for (size_t i = 0; i < pool().max_stream_sockets_per_group() + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));

    auto completer = std::make_unique<MockConnectCompleter>();
    auto ssl = std::make_unique<SSLSocketDataProvider>(completer.get());
    ssl->cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
    ssl->cert_request_info->host_and_port =
        HostPortPair::FromString(kDefaultServerName);
    socket_factory()->AddSSLSocketDataProvider(ssl.get());
    ssl_completers.push(std::move(completer));
    ssl_providers.emplace_back(std::move(ssl));

    auto requester = std::make_unique<StreamRequester>();
    requester->set_destination(kDefaultDestination)
        .set_quic_version(quic_version());
    StreamRequester* raw_requester = requester.get();
    requesters.push(std::move(requester));
    raw_requester->RequestStream(pool());
    ASSERT_FALSE(raw_requester->result().has_value());
  }

  // Fail the first TLS attempt with the client cert needed error. The
  // corresponding AttemptManager enters failing mode.
  std::unique_ptr<MockConnectCompleter> first_ssl_completer =
      std::move(ssl_completers.front());
  ssl_completers.pop();
  first_ssl_completer->Complete(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);

  // Fail the QUIC task. This should not result in attempting TCP-based
  // protocols since we already had the cert error.
  quic_completer.Complete(ERR_QUIC_PROTOCOL_ERROR);

  // Confirm all requests fail with the error.
  while (!requesters.empty()) {
    std::unique_ptr<StreamRequester> requester = std::move(requesters.front());
    requesters.pop();
    requester->WaitForResult();
    EXPECT_THAT(requester->result(),
                Optional(IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED)));
  }

  // Clear `ssl_providers` to avoid dangling pointers to MockConnectCompleters.
  ssl_providers.clear();
}

TEST_F(HttpStreamPoolAttemptManagerTest, OneIPEndPointFailed) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data2.get());

  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v6("2001:db8::1")
                                     .add_v4("192.0.2.1")
                                     .endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, IPEndPointTimedout) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.RequestStream(pool());

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_FALSE(requester.result().has_value());

  FastForwardBy(TcpStreamAttempt::kTcpHandshakeTimeout);
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_TIMED_OUT)));
}

// Tests that when endpoints are slow, other endpoints are attempted.
TEST_F(HttpStreamPoolAttemptManagerTest, IPEndPointsSlow) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());

  auto data1 = std::make_unique<SequencedSocketData>();
  // Make the first and the second attempt stalled.
  data1->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());
  // The third attempt succeeds.
  auto data3 = std::make_unique<SequencedSocketData>();
  data3->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data3.get());

  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v6("2001:db8::1")
                                     .add_v6("2001:db8::2")
                                     .add_v4("192.0.2.1")
                                     .endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  AttemptManager* manager =
      pool().GetGroupForTesting(requester.GetStreamKey())->attempt_manager();
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 1u);
  ASSERT_FALSE(request->completed());

  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 2u);
  ASSERT_EQ(manager->PendingRequestJobCount(), 0u);
  ASSERT_FALSE(request->completed());

  // FastForwardBy() executes non-delayed tasks so the request finishes
  // immediately.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_TRUE(request->completed());
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // Slow attempts should be canceled.
  ASSERT_EQ(pool()
                .GetGroupForTesting(requester.GetStreamKey())
                ->ConnectingStreamSocketCount(),
            0u);
  ASSERT_TRUE(requester.associated_attempt_manager()->is_shutting_down());

  // Reset request so that AttemptManager can complete.
  requester.ResetRequest();

  WaitForAttemptManagerComplete(requester.associated_attempt_manager().get());
  ASSERT_FALSE(requester.associated_attempt_manager());
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       PauseSlowTimerAfterTcpHandshakeForTls) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  MockConnectCompleter tcp_connect_completer1;
  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(&tcp_connect_completer1));
  socket_factory()->AddSocketDataProvider(data1.get());
  // This TLS handshake never finishes.
  auto ssl1 =
      std::make_unique<SSLSocketDataProvider>(SYNCHRONOUS, ERR_IO_PENDING);
  socket_factory()->AddSSLSocketDataProvider(ssl1.get());

  MockConnectCompleter tcp_connect_completer2;
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(&tcp_connect_completer2));
  socket_factory()->AddSocketDataProvider(data2.get());
  auto ssl2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(ssl2.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder()
                         .add_v6("2001:db8::1")
                         .add_v4("192.0.2.1")
                         .endpoint())
      .set_crypto_ready(false)
      .CallOnServiceEndpointsUpdated();
  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .attempt_manager();
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 1u);
  ASSERT_FALSE(requester.result().has_value());

  // Complete TCP handshake after a delay that is less than the connection
  // attempt delay.
  constexpr base::TimeDelta kTcpDelay = base::Milliseconds(30);
  ASSERT_LT(kTcpDelay, HttpStreamPool::GetConnectionAttemptDelay());
  FastForwardBy(kTcpDelay);
  tcp_connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 1u);

  // Fast-forward to the connection attempt delay. Since the in-flight attempt
  // has completed TCP handshake and is waiting for HTTPS RR, the manager
  // shouldn't start another attempt.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 1u);

  // Complete DNS resolution fully.
  endpoint_request->set_crypto_ready(true).CallOnServiceEndpointRequestFinished(
      OK);
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 1u);

  // Fast-forward to the connection attempt delay again. This time the in-flight
  // attempt is still doing TLS handshake, it's treated as slow and the manager
  // should start another attempt.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 2u);

  // Complete the second attempt. The request should finish successfully.
  tcp_connect_completer2.Complete(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Regression test for crbug.com/368187247. Tests that an idle stream socket
// is reused when an in-flight connection attempt is slow.
TEST_F(HttpStreamPoolAttemptManagerTest,
       SlowTimerFiredAfterIdleSocketAvailable) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination("http://a.test").Build();

  MockConnectCompleter connect_completer;
  SequencedSocketData data;
  data.set_connect_data(MockConnect(&connect_completer));
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester(stream_key);
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Create an active text-based stream and release it to create an idle stream.
  // The idle stream should be reused for the in-flight request.
  Group& group = pool().GetOrCreateGroupForTesting(stream_key);
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::make_unique<FakeStreamSocket>(),
      StreamSocketHandle::SocketReuseType::kReusedIdle,
      LoadTimingInfo::ConnectTiming());
  stream.reset();
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  // The in-flight attempt should be canceled.
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  // Fire the slow timer. It should not attempt another connection.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Regression test for crbug.com/414604656. An idle stream could become
// non-usable after a request is blocked by the stream count limits. In such
// case, the request should get a fresh stream.
TEST_F(HttpStreamPoolAttemptManagerTest, IdleStreamNotUsable) {
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  const HttpStreamKey stream_key = StreamKeyBuilder("http://a.test").Build();
  Group& group = pool().GetOrCreateGroupForTesting(stream_key);

  // Create an active text-based stream.
  auto stream_socket = std::make_unique<FakeStreamSocket>();
  FakeStreamSocket* stream_socket_ptr = stream_socket.get();
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::move(stream_socket),
      StreamSocketHandle::SocketReuseType::kReusedIdle,
      LoadTimingInfo::ConnectTiming());
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  data.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(&data);

  // Request another stream. The request should be blocked as the group reached
  // the group limit.
  StreamRequester requester(stream_key);
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Release the first stream that will be kept as an idle stream but will be
  // disconnected later. IsConnected() is called twice to put the stream in the
  // idle stream pool.
  stream_socket_ptr->DisconnectAfterIsConnectedCall(/*count=*/2);
  stream.reset();

  // The request should complete with a fresh stream.
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  ASSERT_FALSE(requester.stream()->IsConnectionReused());
}

TEST_F(HttpStreamPoolAttemptManagerTest, FeatureParamStreamLimits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kHappyEyeballsV3,
      {{std::string(HttpStreamPool::kMaxStreamSocketsPerPoolParamName), "2"},
       {std::string(HttpStreamPool::kMaxStreamSocketsPerGroupParamName), "3"}});
  InitializeSession();
  ASSERT_EQ(pool().max_stream_sockets_per_pool(), 2u);
  ASSERT_EQ(pool().max_stream_sockets_per_group(), 2u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReachedGroupLimit) {
  constexpr size_t kMaxPerGroup = 4;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  // Create streams up to the per-group limit for a destination.
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> data_providers;
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    auto requester = std::make_unique<StreamRequester>();
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());

    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
  }

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  Group& group =
      pool().GetOrCreateGroupForTesting(requesters[0]->GetStreamKey());
  AttemptManager* manager = group.attempt_manager();
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->TcpBasedAttemptCount(), kMaxPerGroup);
  ASSERT_EQ(manager->PendingRequestJobCount(), 0u);

  // This request should not start an attempt as the group reached its limit.
  StreamRequester stalled_requester;
  HttpStreamRequest* stalled_request = stalled_requester.RequestStream(pool());
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());
  data_providers.emplace_back(std::move(data));

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->TcpBasedAttemptCount(), kMaxPerGroup);
  ASSERT_EQ(manager->PendingRequestJobCount(), 1u);
  ASSERT_EQ(stalled_request->GetLoadState(),
            LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET);

  // Finish all in-flight attempts successfully.
  RunUntilIdle();
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 0u);
  ASSERT_EQ(manager->PendingRequestJobCount(), 1u);

  // Release one HttpStream and close it to make non-reusable.
  std::unique_ptr<StreamRequester> released_requester =
      std::move(requesters.back());
  requesters.pop_back();
  std::unique_ptr<HttpStream> released_stream =
      released_requester->ReleaseStream();

  // Need to initialize the HttpStream as HttpBasicStream doesn't disconnect
  // the underlying stream socket when not initialized.
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  released_stream->RegisterRequest(&request_info);
  released_stream->InitializeStream(/*can_send_early=*/false,
                                    RequestPriority::IDLE, NetLogWithSource(),
                                    base::DoNothing());

  released_stream->Close(/*not_reusable=*/true);
  released_stream.reset();

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 1u);
  ASSERT_EQ(manager->PendingRequestJobCount(), 0u);

  RunUntilIdle();

  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group.ActiveStreamSocketCount(), kMaxPerGroup);
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 0u);
  ASSERT_EQ(manager->PendingRequestJobCount(), 0u);
  ASSERT_TRUE(stalled_request->completed());
  std::unique_ptr<HttpStream> stream = stalled_requester.ReleaseStream();
  ASSERT_TRUE(stream);
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  const HttpStreamKey key_a(url::SchemeHostPort("http", "a.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  const HttpStreamKey key_b(url::SchemeHostPort("http", "b.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  // Create HttpStreams up to the group limit in group A.
  Group& group_a = pool().GetOrCreateGroupForTesting(key_a);
  std::vector<std::unique_ptr<HttpStream>> streams_a;
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    streams_a.emplace_back(group_a.CreateTextBasedStream(
        std::make_unique<FakeStreamSocket>(),
        StreamSocketHandle::SocketReuseType::kUnused,
        LoadTimingInfo::ConnectTiming()));
  }

  ASSERT_FALSE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());
  ASSERT_TRUE(group_a.ReachedMaxStreamLimit());
  ASSERT_EQ(pool().TotalActiveStreamCount(), kMaxPerGroup);
  ASSERT_EQ(group_a.ActiveStreamSocketCount(), kMaxPerGroup);

  resolver()
      ->ConfigureDefaultResolution()
      .add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // Create a HttpStream in group B. It should not be blocked because both
  // per-group and per-pool limits are not reached yet.
  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data1.get());

  StreamRequester requester1(key_b);
  requester1.RequestStream(pool());
  requester1.WaitForResult();
  ASSERT_TRUE(requester1.result().has_value());

  // The pool reached the limit, but it doesn't have any blocked request. Group
  // A reached the group limit. Group B doesn't reach the group limit.
  Group& group_b = pool().GetOrCreateGroupForTesting(key_b);
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());
  ASSERT_TRUE(group_a.ReachedMaxStreamLimit());
  ASSERT_FALSE(group_b.ReachedMaxStreamLimit());

  // Create another HttpStream in group B. It should be blocked because the pool
  // reached limit, event when group B doesn't reach its limit.
  StreamRequester requester2(key_b);
  HttpStreamRequest* request2 = requester2.RequestStream(pool());
  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data2.get());
  ASSERT_EQ(request2->GetLoadState(),
            LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL);

  RunUntilIdle();
  ASSERT_FALSE(request2->completed());
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_TRUE(pool().IsPoolStalled());
  ASSERT_EQ(requester2.associated_attempt_manager()->TcpBasedAttemptCount(),
            0u);
  ASSERT_EQ(requester2.associated_attempt_manager()->PendingRequestJobCount(),
            1u);

  // Release one HttpStream from group A. It should unblock the in-flight
  // request in group B.
  std::unique_ptr<HttpStream> released_stream = std::move(streams_a.back());
  streams_a.pop_back();
  released_stream.reset();
  requester2.WaitForResult();

  ASSERT_TRUE(request2->completed());
  ASSERT_EQ(requester2.associated_attempt_manager()->PendingRequestJobCount(),
            0u);
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ReachedPoolLimitHighPriorityGroupFirst) {
  constexpr size_t kMaxPerGroup = 1;
  constexpr size_t kMaxPerPool = 2;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  // Create 4 requests with different destinations and priorities.
  constexpr struct Item {
    std::string_view host;
    std::string_view ip_address;
    RequestPriority priority;
  } items[] = {
      {"a.test", "192.0.2.1", RequestPriority::IDLE},
      {"b.test", "192.0.2.2", RequestPriority::IDLE},
      {"c.test", "192.0.2.3", RequestPriority::LOWEST},
      {"d.test", "192.0.2.4", RequestPriority::HIGHEST},
  };

  std::vector<base::WeakPtr<FakeServiceEndpointRequest>> endpoint_requests;
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> socket_datas;
  for (const auto& [host, ip_address, priority] : items) {
    base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
        resolver()->AddFakeRequest();
    endpoint_request->add_endpoint(
        ServiceEndpointBuilder().add_v4(ip_address).endpoint());
    endpoint_requests.emplace_back(endpoint_request);

    auto requester = std::make_unique<StreamRequester>();
    requester->set_destination(url::SchemeHostPort("http", host, 80))
        .set_priority(priority);
    requesters.emplace_back(std::move(requester));

    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    socket_datas.emplace_back(std::move(data));
  }

  // Complete the first two requests to reach the pool's limit.
  for (size_t i = 0; i < kMaxPerPool; ++i) {
    HttpStreamRequest* request = requesters[i]->RequestStream(pool());
    endpoint_requests[i]->CallOnServiceEndpointRequestFinished(OK);
    RunUntilIdle();
    ASSERT_TRUE(request->completed());
  }

  ASSERT_TRUE(pool().ReachedMaxStreamLimit());

  // Start the remaining requests. These requests should be blocked.
  HttpStreamRequest* request_c = requesters[2]->RequestStream(pool());
  endpoint_requests[2]->CallOnServiceEndpointRequestFinished(OK);

  HttpStreamRequest* request_d = requesters[3]->RequestStream(pool());
  endpoint_requests[3]->CallOnServiceEndpointRequestFinished(OK);

  RunUntilIdle();

  ASSERT_FALSE(request_c->completed());
  ASSERT_FALSE(request_d->completed());

  // Release the HttpStream from group A. It should unblock group D, which has
  // higher priority than group C.
  std::unique_ptr<HttpStream> stream_a = requesters[0]->ReleaseStream();
  stream_a.reset();

  RunUntilIdle();

  ASSERT_FALSE(request_c->completed());
  ASSERT_TRUE(request_d->completed());

  // Release the HttpStream from group B. It should unblock group C.
  std::unique_ptr<HttpStream> stream_b = requesters[1]->ReleaseStream();
  stream_b.reset();

  RunUntilIdle();

  ASSERT_TRUE(request_c->completed());
}

// Regression test for crbug.com/368164182. Tests that the per-group limit is
// respected when there is an idle stream socket.
TEST_F(HttpStreamPoolAttemptManagerTest,
       ReachedPerGroupLimitWithIdleStreamSocket) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination("http://a.test").Build();

  Group& group = pool().GetOrCreateGroupForTesting(stream_key);

  // Create an active text-based stream and release it to create an idle stream.
  std::unique_ptr<HttpStream> stream = group.CreateTextBasedStream(
      std::make_unique<FakeStreamSocket>(),
      StreamSocketHandle::SocketReuseType::kReusedIdle,
      LoadTimingInfo::ConnectTiming());
  stream.reset();

  // Create requests up to the per-group limit + 1. Active stream counts for the
  // group should not exceed the per-group limit.
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (size_t i = 0; i < pool().max_stream_sockets_per_group() + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));

    auto requester = std::make_unique<StreamRequester>(stream_key);
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());
    ASSERT_FALSE(raw_requester->result().has_value());
    ASSERT_LE(group.ActiveStreamSocketCount(),
              pool().max_stream_sockets_per_group());
  }

  for (const auto& requester : requesters) {
    requester->WaitForResult();
    EXPECT_THAT(requester->result(), Optional(IsOk()));
    // Release the stream to unblock other requests.
    requester->ReleaseStream();
  }
}

TEST_F(HttpStreamPoolAttemptManagerTest, RequestStreamIdleStreamSocket) {
  StreamRequester requester;
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  HttpStreamRequest* request = requester.RequestStream(pool());
  RunUntilIdle();
  ASSERT_TRUE(request->completed());

  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
}

// Tests that the group and pool limits are ignored when all requests set
// LOAD_IGNORE_LIMITS.
TEST_F(HttpStreamPoolAttemptManagerTest, IgnoreLimitsAll) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  std::vector<std::unique_ptr<StreamRequester>> requesters;
  std::vector<std::unique_ptr<SequencedSocketData>> data_providers;

  for (size_t i = 0; i < kMaxPerPool + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
    auto requester = std::make_unique<StreamRequester>();
    requester->set_load_flags(LOAD_IGNORE_LIMITS).RequestStream(pool());
    requester->WaitForResult();
    EXPECT_THAT(requester->result(), Optional(IsOk()));
    requesters.emplace_back(std::move(requester));
  }
}

// Tests that the group and pool limits are ignored for requests that set
// LOAD_IGNORE_LIMITS, but once these requests are completed, subsequent
// normal requests respect group and pool limits.
TEST_F(HttpStreamPoolAttemptManagerTest, IgnoreAndRespectLimits) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  std::vector<std::unique_ptr<SequencedSocketData>> data_providers;

  std::vector<std::unique_ptr<StreamRequester>> limit_ignoring_requesters;
  for (size_t i = 0; i < kMaxPerPool + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
    auto requester = std::make_unique<StreamRequester>();
    requester->set_load_flags(LOAD_IGNORE_LIMITS).RequestStream(pool());
    requester->WaitForResult();
    // Requests should not be blocked even the pool/group reach limits.
    EXPECT_THAT(requester->result(), Optional(IsOk()));
    limit_ignoring_requesters.emplace_back(std::move(requester));
  }

  std::list<std::unique_ptr<StreamRequester>> limit_respecting_requesters;
  for (size_t i = 0; i < kMaxPerPool + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
    auto requester = std::make_unique<StreamRequester>();
    requester->RequestStream(pool());
    FastForwardUntilNoTasksRemain();
    // Requests should be blocked.
    ASSERT_FALSE(requester->result().has_value());
    limit_respecting_requesters.emplace_back(std::move(requester));
  }

  // Destroy requests that ignored limits to unblock requests that respect
  // limits.
  limit_ignoring_requesters.clear();

  // Check requests that respect limits complete.
  while (!limit_respecting_requesters.empty()) {
    limit_respecting_requesters.front()->WaitForResult();
    EXPECT_THAT(limit_respecting_requesters.front()->result(),
                Optional(IsOk()));
    limit_respecting_requesters.pop_front();
  }
}

// Tests that the group and pool limits are ignored for requests that set
// LOAD_IGNORE_LIMITS, but once these requests are completed, in-flight attempts
// are canceled until the active stream count goes down to the limits.
TEST_F(HttpStreamPoolAttemptManagerTest,
       IgnoreAndRespectLimitsCancelInflightAttempt) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  const HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination("http://a.test").Build();

  std::list<std::unique_ptr<SequencedSocketData>> data_providers;

  std::list<std::unique_ptr<StreamRequester>> limit_ignoring_requesters;
  for (size_t i = 0; i < kMaxPerPool + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
    auto requester = std::make_unique<StreamRequester>(stream_key);
    requester->set_load_flags(LOAD_IGNORE_LIMITS)
        .set_priority(RequestPriority::HIGHEST)
        .RequestStream(pool());
    limit_ignoring_requesters.emplace_back(std::move(requester));
  }

  std::list<std::unique_ptr<StreamRequester>> limit_respecting_requesters;
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
    auto requester = std::make_unique<StreamRequester>(stream_key);
    requester->set_priority(RequestPriority::LOW).RequestStream(pool());
    limit_respecting_requesters.emplace_back(std::move(requester));
  }

  Group& group = pool().GetOrCreateGroupForTesting(stream_key);
  ASSERT_GT(group.attempt_manager()->TcpBasedAttemptCount(), kMaxPerGroup);

  // Complete requests that ignore limits.
  while (!limit_ignoring_requesters.empty()) {
    limit_ignoring_requesters.front()->WaitForResult();
    EXPECT_THAT(limit_ignoring_requesters.front()->result(), Optional(IsOk()));
    limit_ignoring_requesters.pop_front();
  }

  ASSERT_LE(group.ActiveStreamSocketCount(), kMaxPerGroup);

  // Complete requests that respect limits.
  while (!limit_respecting_requesters.empty()) {
    limit_respecting_requesters.front()->WaitForResult();
    EXPECT_THAT(limit_respecting_requesters.front()->result(),
                Optional(IsOk()));
    limit_respecting_requesters.pop_front();
  }

  // Ensure that the total connecting stream count is decremented appropriately.
  ASSERT_EQ(pool().TotalConnectingStreamCount(), 0u);
}

// Regression test for crbug.com/397535403.
// Tests that limit ignoring requests are allowed to exceeds pool's default
// limit.
TEST_F(HttpStreamPoolAttemptManagerTest, IgnoreLimitsExceedsPoolDefaultLimit) {
  const HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination("http://a.test").Build();

  std::list<std::unique_ptr<SequencedSocketData>> data_providers;
  std::list<std::unique_ptr<StreamRequester>> requesters;

  for (size_t i = 0; i < HttpStreamPool::kDefaultMaxStreamSocketsPerPool + 1;
       ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
    auto requester = std::make_unique<StreamRequester>(stream_key);
    requester->set_load_flags(LOAD_IGNORE_LIMITS).RequestStream(pool());
    requesters.emplace_back(std::move(requester));
  }

  // Complete requests.
  while (!requesters.empty()) {
    requesters.front()->WaitForResult();
    EXPECT_THAT(requesters.front()->result(), Optional(IsOk()));
    requesters.pop_front();
  }
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       IgnoreLimitsExceedsPoolLimitCloseIdleStreams) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 6;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  const HttpStreamKey stream_key_a =
      StreamKeyBuilder().set_destination("http://a.test").Build();
  const HttpStreamKey stream_key_b =
      StreamKeyBuilder().set_destination("http://b.test").Build();

  // Add some idle streams for b.test.
  Group& group_b = pool().GetOrCreateGroupForTesting(stream_key_b);
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    group_b.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  }

  // Request streams ignoring limits. This should close idle streams.
  std::list<std::unique_ptr<SequencedSocketData>> data_providers;
  std::list<std::unique_ptr<StreamRequester>> requesters;
  for (size_t i = 0; i < kMaxPerPool + 1; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    data_providers.emplace_back(std::move(data));
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
    auto requester = std::make_unique<StreamRequester>(stream_key_a);
    requester->set_load_flags(LOAD_IGNORE_LIMITS).RequestStream(pool());
    requesters.emplace_back(std::move(requester));
  }
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 0u);

  // Complete requests.
  while (!requesters.empty()) {
    requesters.front()->WaitForResult();
    EXPECT_THAT(requesters.front()->result(), Optional(IsOk()));
    requesters.pop_front();
  }
}

TEST_F(HttpStreamPoolAttemptManagerTest, UseIdleStreamSocketAfterRelease) {
  StreamRequester requester;
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());

  // Create HttpStreams up to the group's limit.
  std::vector<std::unique_ptr<HttpStream>> streams;
  for (size_t i = 0; i < pool().max_stream_sockets_per_group(); ++i) {
    std::unique_ptr<HttpStream> http_stream = group.CreateTextBasedStream(
        std::make_unique<FakeStreamSocket>(),
        StreamSocketHandle::SocketReuseType::kUnused,
        LoadTimingInfo::ConnectTiming());
    streams.emplace_back(std::move(http_stream));
  }
  ASSERT_EQ(group.ActiveStreamSocketCount(),
            pool().max_stream_sockets_per_group());
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);

  // Request a stream. The request should be blocked.
  resolver()->AddFakeRequest();
  HttpStreamRequest* request = requester.RequestStream(pool());
  RunUntilIdle();
  AttemptManager* manager = group.attempt_manager();
  ASSERT_FALSE(request->completed());
  ASSERT_EQ(manager->PendingRequestJobCount(), 1u);

  // Release an active HttpStream. The underlying StreamSocket should be used
  // to the pending request.
  std::unique_ptr<HttpStream> released_stream = std::move(streams.back());
  streams.pop_back();

  released_stream.reset();
  requester.WaitForResult();
  ASSERT_TRUE(request->completed());
  ASSERT_EQ(manager->PendingRequestJobCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       CloseIdleStreamAttemptConnectionReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 2;
  constexpr size_t kMaxPerPool = 3;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  const HttpStreamKey key_a(url::SchemeHostPort("http", "a.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  const HttpStreamKey key_b(url::SchemeHostPort("http", "b.test", 80),
                            PRIVACY_MODE_DISABLED, SocketTag(),
                            NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                            /*disable_cert_network_fetches=*/false);

  // Add idle streams up to the group's limit in group A.
  Group& group_a = pool().GetOrCreateGroupForTesting(key_a);
  for (size_t i = 0; i < kMaxPerGroup; ++i) {
    group_a.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  }
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 2u);
  ASSERT_FALSE(pool().ReachedMaxStreamLimit());

  // Create an HttpStream in group B. The pool should reach its limit.
  Group& group_b = pool().GetOrCreateGroupForTesting(key_b);
  std::unique_ptr<HttpStream> stream1 = group_b.CreateTextBasedStream(
      std::make_unique<FakeStreamSocket>(),
      StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());

  // Request a stream in group B. The request should close an idle stream in
  // group A.
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  StreamRequester requester;
  HttpStreamRequest* request = requester.RequestStream(pool());
  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  endpoint_request->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  ASSERT_TRUE(request->completed());
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ProcessPendingRequestDnsResolutionOngoing) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());

  StreamRequester requester;
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // This should not enter an infinite loop.
  pool().ProcessPendingRequestsInGroups();

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Tests that all in-flight requests and connection attempts are canceled
// when an IP address change event happens.
TEST_F(HttpStreamPoolAttemptManagerTest,
       CancelAttemptAndRequestsOnIPAddressChange) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      resolver()->AddFakeRequest();
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      resolver()->AddFakeRequest();

  auto data1 = std::make_unique<SequencedSocketData>();
  data1->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data1.get());

  auto data2 = std::make_unique<SequencedSocketData>();
  data2->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data2.get());

  StreamRequester requester1;
  requester1.set_destination("https://a.test").RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination("https://b.test").RequestStream(pool());

  endpoint_request1->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint());
  endpoint_request1->CallOnServiceEndpointRequestFinished(OK);
  endpoint_request2->add_endpoint(
      ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint());
  endpoint_request2->CallOnServiceEndpointRequestFinished(OK);

  AttemptManager* manager1 =
      pool()
          .GetOrCreateGroupForTesting(requester1.GetStreamKey())
          .attempt_manager();
  AttemptManager* manager2 =
      pool()
          .GetOrCreateGroupForTesting(requester2.GetStreamKey())
          .attempt_manager();
  ASSERT_EQ(manager1->RequestJobCount(), 1u);
  ASSERT_EQ(manager1->TcpBasedAttemptCount(), 1u);
  ASSERT_EQ(manager2->RequestJobCount(), 1u);
  ASSERT_EQ(manager2->TcpBasedAttemptCount(), 1u);

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();
  ASSERT_EQ(manager1->RequestJobCount(), 0u);
  ASSERT_EQ(manager1->TcpBasedAttemptCount(), 0u);
  ASSERT_EQ(manager2->RequestJobCount(), 0u);
  ASSERT_EQ(manager2->TcpBasedAttemptCount(), 0u);
  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
}

// Tests that the network change error is reported even when a different error
// has already happened.
TEST_F(HttpStreamPoolAttemptManagerTest, IPAddressChangeAfterNeedsClientAuth) {
  // Set the per-group limit to one to allow only one attempt.
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const url::SchemeHostPort kDestination(GURL("https://a.test"));

  auto data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(data.get());
  SSLSocketDataProvider ssl(SYNCHRONOUS, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromSchemeHostPort(kDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  RunUntilIdle();
  EXPECT_THAT(requester1.result(),
              Optional(IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED)));
  EXPECT_THAT(requester2.result(),
              Optional(IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SSLConfigChangedCloseIdleStream) {
  StreamRequester requester;
  requester.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);

  ssl_config_service()->NotifySSLContextConfigChange();
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);

  // Ensure the group is destroyed.
  FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(pool().GetGroupForTesting(requester.GetStreamKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SSLConfigChangedReleasedStreamGenerationOutdated) {
  StreamRequester requester;
  requester.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  std::unique_ptr<HttpStream> stream =
      group.CreateTextBasedStream(std::make_unique<FakeStreamSocket>(),
                                  StreamSocketHandle::SocketReuseType::kUnused,
                                  LoadTimingInfo::ConnectTiming());
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  ssl_config_service()->NotifySSLContextConfigChange();
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  // Release the HttpStream, the underlying StreamSocket should not be pooled
  // as an idle stream since the generation is different.
  stream.reset();
  ASSERT_FALSE(pool().GetGroupForTesting(requester.GetStreamKey()));
}

// Tests that a group and corresponding attempt manager are destroyed after
// cancelling in-flight attempts due to an SSLConfig change when there are no
// jobs.
TEST_F(HttpStreamPoolAttemptManagerTest, CancelAttemptOnSSLConfigChangeNoJobs) {
  constexpr size_t kNumRequest = 2;

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  const HttpStreamKey stream_key = StreamKeyBuilder().Build();
  std::vector<std::unique_ptr<MockConnectCompleter>> completers;
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  for (size_t i = 0; i < kNumRequest; ++i) {
    auto completer = std::make_unique<MockConnectCompleter>();
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(completer.get()));
    socket_factory()->AddSocketDataProvider(data.get());
    completers.emplace_back(std::move(completer));
    datas.emplace_back(std::move(data));

    auto requester = std::make_unique<StreamRequester>(stream_key);
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());
    ASSERT_FALSE(raw_requester->result().has_value());
  }
  AttemptManager* manager =
      pool().GetGroupForTesting(stream_key)->attempt_manager();
  ASSERT_EQ(manager->RequestJobCount(), 2u);
  ASSERT_EQ(manager->NotifiedRequestJobCount(), 0u);
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 2u);

  // Trigger slow timers.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());

  // Cancel requests. This should remove all jobs from the corresponding group.
  // Ensure that the job and attempt manager are still alive since there are
  // in-flight attempts.
  requesters.clear();
  manager = pool().GetGroupForTesting(stream_key)->attempt_manager();
  ASSERT_TRUE(manager);
  ASSERT_EQ(manager->RequestJobCount(), 0u);
  ASSERT_EQ(manager->NotifiedRequestJobCount(), 0u);
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 2u);

  // Trigger an SSLConfig change. This should cancel in-flight attempts.
  ssl_config_service()->NotifySSLContextConfigChange();

  // Run the cleanup task. The corresponding group and attempt manager should be
  // destroyed.
  FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(pool().GetGroupForTesting(stream_key));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SSLConfigForServersChanged) {
  // Create idle streams in group A and group B.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  Group& group_a =
      pool().GetOrCreateGroupForTesting(requester_a.GetStreamKey());
  group_a.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 1u);

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");
  Group& group_b =
      pool().GetOrCreateGroupForTesting(requester_b.GetStreamKey());
  group_b.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 1u);

  // Simulate an SSLConfigForServers change event for group A. The idle stream
  // in group A should be gone but the idle stream in group B should remain.
  pool().OnSSLConfigForServersChanged({HostPortPair::FromSchemeHostPort(
      requester_a.GetStreamKey().destination())});
  ASSERT_EQ(group_a.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyAvailableSession) {
  StreamRequester requester;
  requester.set_destination("https://a.test")
      .set_enable_ip_based_pooling(false);

  CreateFakeSpdySession(requester.GetStreamKey());
  requester.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Test that setting the priority for a request that will be served via an
// existing SPDY session doesn't crash the network service.
TEST_F(HttpStreamPoolAttemptManagerTest, ChangePriorityForPooledStreamRequest) {
  StreamRequester requester;
  requester.set_destination("https://a.test");

  CreateFakeSpdySession(requester.GetStreamKey());

  HttpStreamRequest* request = requester.RequestStream(pool());
  request->SetPriority(RequestPriority::HIGHEST);
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  // HttpStream{,Request} don't provide a way to get its priority.
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyOk) {
  // Create two requests for the same destination. Once a connection is
  // established and is negotiated to use H2, another connection attempts should
  // be canceled and all requests should receive HttpStreams on top of the
  // SpdySession.

  constexpr size_t kNumRequests = 2;
  const HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination("https://a.test").Build();

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  std::vector<std::unique_ptr<SequencedSocketData>> socket_datas;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssls;
  std::vector<std::unique_ptr<StreamRequester>> requesters;

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  for (size_t i = 0; i < kNumRequests; ++i) {
    auto data = std::make_unique<SequencedSocketData>(reads, writes);
    socket_factory()->AddSocketDataProvider(data.get());
    socket_datas.emplace_back(std::move(data));
    auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    ssl->next_proto = NextProto::kProtoHTTP2;
    socket_factory()->AddSSLSocketDataProvider(ssl.get());
    ssls.emplace_back(std::move(ssl));

    auto requester = std::make_unique<StreamRequester>(stream_key);
    requester->set_enable_ip_based_pooling(false).RequestStream(pool());
    requesters.emplace_back(std::move(requester));
  }

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);

  for (auto& requester : requesters) {
    requester->WaitForResult();
    ASSERT_TRUE(requester->result().has_value());
    EXPECT_THAT(requester->result(), Optional(IsOk()));
  }
  Group& group =
      pool().GetOrCreateGroupForTesting(requesters[0]->GetStreamKey());
  ASSERT_EQ(group.ConnectingStreamSocketCount(), 0u);
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_EQ(pool().TotalConnectingStreamCount(), 0u);
  ASSERT_TRUE(http_server_properties()->GetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyCreateSessionFail) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  // Set an invalid ALPS to make SPDY session creation fail.
  ssl->peer_application_settings = "invalid alps";
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();

  EXPECT_THAT(requester.result(), Optional(IsError(ERR_HTTP2_PROTOCOL_ERROR)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DoNotUseSpdySessionForHttpRequest) {
  constexpr std::string_view kHttpsDestination = "https://www.example.com";
  constexpr std::string_view kHttpDestination = "http://www.example.com";

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto h2_data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(h2_data.get());
  auto h2_ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  h2_ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(h2_ssl.get());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester_https;
  requester_https.set_destination(kHttpsDestination).RequestStream(pool());
  HttpStreamKey stream_key = requester_https.GetStreamKey();
  RunUntilIdle();
  EXPECT_THAT(requester_https.result(), Optional(IsOk()));
  EXPECT_EQ(requester_https.negotiated_protocol(), NextProto::kProtoHTTP2);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.CalculateSpdySessionKey(), /*enable_ip_based_pooling=*/true,
      /*is_websocket=*/false));

  // Request a stream for http (not https). The second request should use
  // HTTP/1.1 and should not use the existing SPDY session.
  auto h1_data = std::make_unique<SequencedSocketData>();
  socket_factory()->AddSocketDataProvider(h1_data.get());
  SSLSocketDataProvider h1_ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&h1_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester_http;
  requester_http.set_destination(kHttpDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_http.result(), Optional(IsOk()));
  EXPECT_NE(requester_http.negotiated_protocol(), NextProto::kProtoHTTP2);
}

TEST_F(HttpStreamPoolAttemptManagerTest, CloseIdleSpdySessionWhenPoolStalled) {
  pool().set_max_stream_sockets_per_group_for_testing(1u);
  pool().set_max_stream_sockets_per_pool_for_testing(1u);

  constexpr std::string_view kDestinationA = "https://a.test";
  constexpr std::string_view kDestinationB = "https://b.test";

  // Create an idle SPDY session for `kDestinationA`. This session should be
  // closed when a request is created for `kDestinationB`.
  const HttpStreamKey stream_key_a =
      StreamKeyBuilder().set_destination(kDestinationA).Build();
  CreateFakeSpdySession(stream_key_a);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key_a.CalculateSpdySessionKey(), /*enable_ip_based_pooling=*/true,
      /*is_websocket=*/false));

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester_b;
  requester_b.set_destination(kDestinationB).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  EXPECT_EQ(requester_b.negotiated_protocol(), NextProto::kProtoHTTP2);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      requester_b.GetStreamKey().CalculateSpdySessionKey(),
      /*enable_ip_based_pooling=*/true,
      /*is_websocket=*/false));
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key_a.CalculateSpdySessionKey(), /*enable_ip_based_pooling=*/true,
      /*is_websocket=*/false));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdySessionBecomeUnavailable) {
  const HttpStreamKey stream_key =
      StreamKeyBuilder(kDefaultDestination).Build();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data1(reads, writes);
  socket_factory()->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl1(ASYNC, OK);
  ssl1.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl1);

  // The first request creates an SPDY session.
  StreamRequester requester1(stream_key);
  requester1.RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_EQ(requester1.negotiated_protocol(), NextProto::kProtoHTTP2);

  // Close the SPDY session.
  spdy_session_pool()->CloseAllSessions();

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data2(reads, writes);
  socket_factory()->AddSocketDataProvider(&data2);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl2);

  // The second request creates another SPDY session.
  StreamRequester requester2(stream_key);
  requester2.RequestStream(pool());
  EXPECT_FALSE(requester2.result().has_value());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_EQ(requester2.negotiated_protocol(), NextProto::kProtoHTTP2);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdySessionBecomeUnavailablePreconnect) {
  const HttpStreamKey stream_key =
      StreamKeyBuilder(kDefaultDestination).Build();

  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data1(reads, writes);
  socket_factory()->AddSocketDataProvider(&data1);
  SSLSocketDataProvider ssl1(ASYNC, OK);
  ssl1.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl1);

  // The first preconnect creates an SPDY session.
  Preconnector preconnector1(kDefaultDestination);
  preconnector1.Preconnect(pool());
  preconnector1.WaitForResult();
  EXPECT_THAT(preconnector1.result(), Optional(IsOk()));

  // Close the SPDY session.
  spdy_session_pool()->CloseAllSessions();
  FastForwardUntilNoTasksRemain();

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data2(reads, writes);
  socket_factory()->AddSocketDataProvider(&data2);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  ssl2.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl2);

  // The second preconnect creates another SPDY session.
  Preconnector preconnector2(kDefaultDestination);
  preconnector2.Preconnect(pool());
  preconnector2.WaitForResult();
  EXPECT_THAT(preconnector2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 1;
  constexpr size_t kMaxPerPool = 2;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  // Create SPDY sessions up to the pool limit. Initialize streams to make
  // SPDY sessions active.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  base::WeakPtr<SpdySession> spdy_session_a = CreateFakeSpdySession(
      requester_a.GetStreamKey(), MakeIPEndPoint("192.0.2.1"));
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream_a = requester_a.ReleaseStream();
  HttpRequestInfo request_info_a;
  request_info_a.url = GURL("https://a.test");
  request_info_a.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream_a->RegisterRequest(&request_info_a);
  stream_a->InitializeStream(/*can_send_early=*/false, DEFAULT_PRIORITY,
                             NetLogWithSource(), base::DoNothing());

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");
  CreateFakeSpdySession(requester_b.GetStreamKey(),
                        MakeIPEndPoint("192.0.2.2"));
  requester_b.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream_b = requester_b.ReleaseStream();
  HttpRequestInfo request_info_b;
  request_info_b.url = GURL("https://b.test");
  request_info_b.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream_b->RegisterRequest(&request_info_b);
  stream_b->InitializeStream(/*can_send_early=*/false, DEFAULT_PRIORITY,
                             NetLogWithSource(), base::DoNothing());

  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());

  // Request a stream in group C. It should be blocked.
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester_c;
  requester_c.set_destination("https://c.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  Group& group_c =
      pool().GetOrCreateGroupForTesting(requester_c.GetStreamKey());
  ASSERT_EQ(group_c.attempt_manager()->PendingRequestJobCount(), 1u);
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_TRUE(pool().IsPoolStalled());

  // Close the group A's SPDY session. It should unblock the request in group C.
  spdy_session_a->CloseSessionOnError(ERR_ABORTED,
                                      /*description=*/"for testing");
  RunUntilIdle();
  EXPECT_THAT(requester_c.result(), Optional(IsOk()));
  ASSERT_TRUE(pool().ReachedMaxStreamLimit());
  ASSERT_FALSE(pool().IsPoolStalled());

  // Need to close HttpStreams before finishing this test due to the DCHECK in
  // the destructor of SpdyHttpStream.
  // TODO(crbug.com/346835898): Figure out a way not to rely on this behavior,
  // or fix SpdySessionStream somehow.
  stream_a->Close(/*not_reusable=*/true);
  stream_b->Close(/*not_reusable=*/true);
}

// In the following SPDY IP-based pooling tests, we use spdy_pooling.pem that
// has "www.example.org" and "example.test" as alternate names.

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyMatchingIpSessionOk) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionBecomeUnavailableBeforeNotify) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  // Add a SpdySession for www.example.org.
  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  requester_a.WaitForResult();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  // Data for the second request.
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  // Create the second request to example.test. It will finds the matching
  // SPDY session, but the task to use the session runs asynchronously, so it
  // hasn't run yet.
  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());
  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester_b.result().has_value());

  // Close the session before the second request can try to use it.
  spdy_session_pool()->CloseAllSessions();

  requester_b.WaitForResult();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  EXPECT_EQ(requester_b.negotiated_protocol(), NextProto::kProtoHTTP2);
  // The session was already closed so it's not available.
  ASSERT_FALSE(spdy_session_pool()->FindAvailableSession(
      requester_b.GetStreamKey().CalculateSpdySessionKey(),
      /*enable_ip_based_pooling=*/true,
      /*is_websocket=*/false, NetLogWithSource()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyPreconnectMatchingIpSession) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector_b("https://example.test");
  preconnector_b.Preconnect(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(preconnector_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionAlreadyHaveSession) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  requester_a.WaitForResult();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());

  // Call CallOnServiceEndpointsUpdated(). The corresponding AttemptManager will
  // destroy ServiceEndpointRequest.
  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointsUpdated();
  CHECK(!endpoint_request);

  requester_b.WaitForResult();

  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionDnsResolutionFinishSynchronously) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());
  ASSERT_FALSE(requester_b.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyMatchingIpSessionDisabled) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("192.0.2.1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test")
      .set_enable_ip_based_pooling(false)
      .RequestStream(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionDisabledThenEnabled) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("192.0.2.1", 443);

  // Preparation: Create a SPDY session for www.example.org.
  StreamRequester requester1;
  requester1.set_destination("https://www.example.org");
  CreateFakeSpdySession(requester1.GetStreamKey(), kCommonEndPoint);
  requester1.RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  // The first request for example.test disables IP-based pooling. Set up mock
  // data to fail the request.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester2;
  requester2.set_destination("https://example.test")
      .set_enable_ip_based_pooling(false)
      .RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_FAILED)));
  requester2.ResetRequest();

  // The second request for example.test enables IP-based pooling. The request
  // should use the existing SPDY session.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester3;
  requester3.set_destination("https://example.test")
      .set_enable_ip_based_pooling(true)
      .RequestStream(pool());
  requester3.WaitForResult();
  EXPECT_THAT(requester3.result(), Optional(IsOk()));

  ASSERT_EQ(pool().TotalActiveStreamCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyMatchingIpSessionKeyMismatch) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("192.0.2.1", 443);

  StreamRequester requester_a;
  // Set privacy mode to make SpdySessionKey different.
  requester_a.set_destination("https://www.example.org")
      .set_privacy_mode(PRIVACY_MODE_ENABLED_WITHOUT_CLIENT_CERTS);

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester_b;
  requester_b.set_destination("https://example.test").RequestStream(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionVerifyDomainFailed) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("192.0.2.1", 443);

  StreamRequester requester_a;
  requester_a.set_destination("https://www.example.org");

  CreateFakeSpdySession(requester_a.GetStreamKey(), kCommonEndPoint);
  requester_a.RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester_a.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  // Use a destination that is not listed in spdy_pooling.pem.
  StreamRequester requester_b;
  requester_b.set_destination("https://non-alternative.test")
      .RequestStream(pool());

  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));
  ASSERT_EQ(pool().TotalActiveStreamCount(), 2u);
}

// Regression test for crbug.com/385296757.
// If an IP matching SPDY session is created during the stream attempt delay,
// use that session instead of attempting a new connection after the delay.
TEST_F(HttpStreamPoolAttemptManagerTest,
       SpdyMatchingIpSessionStreamAttemptDelayPassed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .set_crypto_ready(true);

  // QUIC task stalls forever.
  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data->AddSocketDataToFactory(socket_factory());

  StreamRequester requester;
  requester.set_destination("https://example.test")
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // `endpoint_request` notifies that it has updated endpoints.
  endpoint_request->CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester.result().has_value());

  // Create a SPDY session for "https://www.example.org" with an IP address
  // matching the IPv6 address returned by the HostResolver.
  HttpStreamKey stream_key =
      StreamKeyBuilder("https://www.example.org").Build();
  CreateFakeSpdySession(stream_key, kCommonEndPoint);

  // Simulate endpoint resolution complete. The attempt manager performs IP
  // based matching and should find the existing SPDY session.
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  // Trigger stream attempt delay timer. The attempt manager should not make
  // connection attempts but should use the SPDY session.
  FastForwardBy(kDelay);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoHTTP2);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ThrottleAttemptForSpdyBlockSecondAttempt) {
  constexpr std::string_view kDestination = "https://a.test";

  // Set the destination is known to support HTTP/2.
  HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination(kDestination).Build();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  // There should be only one in-flight attempt because attempts are throttled.
  Group& group = pool().GetOrCreateGroupForTesting(requester1.GetStreamKey());
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), 1u);

  // This should not enter an infinite loop.
  pool().ProcessPendingRequestsInGroups();

  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ThrottleAttemptForSpdyDelayPassedHttp2) {
  constexpr std::string_view kDestination = "https://a.test";

  // Set the destination is known to support HTTP/2.
  HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination(kDestination).Build();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockConnectCompleter connect_completer1;
  auto data1 = std::make_unique<SequencedSocketData>(reads, writes);
  data1->set_connect_data(MockConnect(&connect_completer1));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto ssl1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl1->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl1.get());

  MockConnectCompleter connect_completer2;
  auto data2 = std::make_unique<SequencedSocketData>(reads, writes);
  data2->set_connect_data(MockConnect(&connect_completer2));
  socket_factory()->AddSocketDataProvider(data2.get());
  auto ssl2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl2->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl2.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  // There should be only one in-flight attempt because attempts are throttled.
  Group& group = pool().GetOrCreateGroupForTesting(requester1.GetStreamKey());
  ASSERT_EQ(group.ConnectingStreamSocketCount(), 1u);

  FastForwardBy(AttemptManager::kSpdyThrottleDelay);
  ASSERT_EQ(group.ConnectingStreamSocketCount(), 2u);

  connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(group.ConnectingStreamSocketCount(), 0u);

  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       ThrottleAttemptForSpdyDelayPassedHttp1) {
  constexpr std::string_view kDestination = "https://a.test";

  // Set the destination is known to support HTTP/2.
  HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination(kDestination).Build();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester1;
  requester1.set_destination(kDestination).RequestStream(pool());

  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  MockConnectCompleter connect_completer1;
  auto data1 = std::make_unique<SequencedSocketData>(reads, writes);
  data1->set_connect_data(MockConnect(&connect_completer1));
  socket_factory()->AddSocketDataProvider(data1.get());
  auto ssl1 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(ssl1.get());

  MockConnectCompleter connect_completer2;
  auto data2 = std::make_unique<SequencedSocketData>(reads, writes);
  data2->set_connect_data(MockConnect(&connect_completer2));
  socket_factory()->AddSocketDataProvider(data2.get());
  auto ssl2 = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(ssl2.get());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  // There should be only one in-flight attempt because attempts are throttled.
  Group& group = pool().GetOrCreateGroupForTesting(requester1.GetStreamKey());
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), 1u);

  FastForwardBy(AttemptManager::kSpdyThrottleDelay);
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), 2u);

  connect_completer1.Complete(OK);
  RunUntilIdle();
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), 1u);

  connect_completer2.Complete(OK);
  RunUntilIdle();

  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectSpdySessionAvailable) {
  Preconnector preconnector("https://a.test");
  CreateFakeSpdySession(preconnector.GetStreamKey());

  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsOk());
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectActiveStreamsAvailable) {
  Preconnector preconnector("http://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsOk());
  ASSERT_EQ(group.attempt_manager(), nullptr);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectFail) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, ERR_FAILED));
  socket_factory()->AddSocketDataProvider(data.get());

  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), 1u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(*preconnector.result(), IsError(ERR_FAILED));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleStreamsHttp1) {
  constexpr size_t kNumStreams = 2;

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (size_t i = 0; i < kNumStreams; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), kNumStreams);
}

// Test that preconnects don't attempt new streams when the maximum
// preconnect count is less than or equal to the active stream count, for
// compatibility with the non-HEv3 code path.
// TODO(crbug.com/346835898): Revisit this behavior when we obsolete the
// non-HEv3 code path.
TEST_F(HttpStreamPoolAttemptManagerTest,
       PreconnectMultipleStreamsWithActiveOneHttp1) {
  constexpr size_t kNumPreconnectStreams = 2;

  const HttpStreamKey stream_key = StreamKeyBuilder().Build();

  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (size_t i = 0; i < kNumPreconnectStreams; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  resolver()
      ->ConfigureDefaultResolution()
      .add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // Preparation: Create an active stream.
  StreamRequester requester(stream_key);
  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  Group& group = pool().GetOrCreateGroupForTesting(stream_key);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  // Preconnect multiple streams.
  Preconnector preconnector("http://a.test");
  int rv =
      preconnector.set_num_streams(kNumPreconnectStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(),
            kNumPreconnectStreams - 1u);
  ASSERT_FALSE(preconnector.result().has_value());

  preconnector.WaitForResult();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.ActiveStreamSocketCount(), kNumPreconnectStreams);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleStreamsHttp2) {
  constexpr size_t kNumStreams = 2;

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector("https://a.test");

  HttpStreamKey stream_key = preconnector.GetStreamKey();
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), 1u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), 0u);
  ASSERT_TRUE(spdy_session_pool()->HasAvailableSession(
      stream_key.CalculateSpdySessionKey(), /*enable_ip_based_pooling=*/true,
      /*is_websocket=*/false));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectRequireHttp1) {
  constexpr size_t kNumStreams = 2;

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector("https://a.test");

  HttpStreamKey stream_key = preconnector.GetStreamKey();
  http_server_properties()->SetHTTP11Required(
      stream_key.destination(), stream_key.network_anonymization_key());

  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssls;
  for (size_t i = 0; i < kNumStreams; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
    auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    ssl->next_protos_expected_in_ssl_config = {NextProto::kProtoHTTP11};
    socket_factory()->AddSSLSocketDataProvider(ssl.get());
    ssls.emplace_back(std::move(ssl));
  }

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), 2u);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(group.IdleStreamSocketCount(), 2u);
  ASSERT_FALSE(spdy_session_pool()->HasAvailableSession(
      stream_key.CalculateSpdySessionKey(), /*enable_ip_based_pooling=*/true,
      /*is_websocket=*/false));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleStreamsOkAndFail) {
  constexpr size_t kNumStreams = 2;

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  std::vector<MockConnect> connects = {
      {MockConnect(ASYNC, OK), MockConnect(ASYNC, ERR_FAILED)}};
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (const auto& connect : connects) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(connect);
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  preconnector.WaitForResult();
  // Even the second connection attempt will fail, the preconnect request
  // completes successfully when the first attempt succeeded. This behavior is
  // to align the behavior of the non-HEv3 code path.
  // TODO(crbug.com/346835898): Revisit this behavior when we obsolete the
  // non-HEv3 code path.
  EXPECT_THAT(preconnector.result(), Optional(OK));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleStreamsFailAndOk) {
  constexpr size_t kNumStreams = 2;

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  std::vector<MockConnect> connects = {
      {MockConnect(ASYNC, ERR_FAILED), MockConnect(ASYNC, OK)}};
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (const auto& connect : connects) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(connect);
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  ASSERT_EQ(group.attempt_manager()->TcpBasedAttemptCount(), kNumStreams);
  ASSERT_FALSE(preconnector.result().has_value());

  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_FAILED)));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectMultipleRequests) {
  constexpr std::string_view kDestination("http://a.test");

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector1(kDestination);

  std::array<MockConnectCompleter, 2> completers;
  std::vector<MockConnect> connects = {
      {MockConnect(&completers[0]), MockConnect(&completers[1])}};
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  for (const auto& connect : connects) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(connect);
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));
  }

  int rv = preconnector1.set_num_streams(1).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  ASSERT_FALSE(preconnector1.result().has_value());

  completers[0].Complete(OK);
  preconnector1.WaitForResult();
  EXPECT_THAT(preconnector1.result(), Optional(IsOk()));

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector2(kDestination);
  rv = preconnector2.set_num_streams(2).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  completers[1].Complete(OK);
  preconnector2.WaitForResult();
  EXPECT_THAT(preconnector2.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectReachedGroupLimit) {
  constexpr size_t kMaxPerGroup = 1;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);

  constexpr size_t kNumStreams = 2;

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector("http://a.test");

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  int rv = preconnector.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  Group& group = pool().GetOrCreateGroupForTesting(preconnector.GetStreamKey());
  EXPECT_THAT(preconnector.result(),
              Optional(IsError(ERR_PRECONNECT_MAX_SOCKET_LIMIT)));
  ASSERT_EQ(group.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectReachedPoolLimit) {
  constexpr size_t kMaxPerGroup = 1;
  constexpr size_t kMaxPerPool = 2;
  pool().set_max_stream_sockets_per_group_for_testing(kMaxPerGroup);
  pool().set_max_stream_sockets_per_pool_for_testing(kMaxPerPool);

  constexpr size_t kNumStreams = 2;

  auto key_a = StreamKeyBuilder("http://a.test").Build();
  pool().GetOrCreateGroupForTesting(key_a).CreateTextBasedStream(
      std::make_unique<FakeStreamSocket>(),
      StreamSocketHandle::SocketReuseType::kUnused,
      LoadTimingInfo::ConnectTiming());

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  Preconnector preconnector_b("http://b.test");

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  int rv = preconnector_b.set_num_streams(kNumStreams).Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  Group& group_b =
      pool().GetOrCreateGroupForTesting(preconnector_b.GetStreamKey());
  EXPECT_THAT(preconnector_b.result(),
              Optional(IsError(ERR_PRECONNECT_MAX_SOCKET_LIMIT)));
  ASSERT_EQ(group_b.IdleStreamSocketCount(), 1u);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       RequestStreamAndPreconnectWhileFailing) {
  constexpr std::string_view kDestination = "http://a.test";

  // Add two fake DNS resolutions (one for failing case, another is for success
  // case).
  for (size_t i = 0; i < 2; ++i) {
    base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
        resolver()->AddFakeRequest();
    endpoint_request
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
  }

  auto failed_data = std::make_unique<SequencedSocketData>();
  failed_data->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(failed_data.get());

  auto success_data = std::make_unique<SequencedSocketData>();
  success_data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(success_data.get());

  HttpStreamKey stream_key =
      StreamKeyBuilder().set_destination(kDestination).Build();

  StreamRequester requester1(stream_key);
  requester1.RequestStream(pool());

  Group* group = pool().GetGroupForTesting(stream_key);

  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsError(ERR_CONNECTION_RESET)));
  EXPECT_TRUE(requester1.associated_attempt_manager()->is_shutting_down());

  // The first request isn't destroyed yet so the failing AttemptManager is
  // still alive. A request that comes during a failure should use a new
  // AttemptManager.
  StreamRequester requester2(stream_key);
  HttpStreamRequest* request2 = requester2.RequestStream(pool());
  ASSERT_FALSE(requester2.result().has_value());
  ASSERT_NE(requester1.associated_attempt_manager().get(),
            group->attempt_manager());
  ASSERT_EQ(group->attempt_manager()->TcpBasedAttemptCount(), 1u);
  EXPECT_EQ(request2->GetLoadState(), LOAD_STATE_CONNECTING);

  // Preconnect should succeed immediately as the active AttemptManager has
  // a TcpBasedAttempt.
  Preconnector preconnector1(kDestination);
  EXPECT_THAT(preconnector1.Preconnect(pool()), IsOk());

  // Destroy the failed request. This should destroy the failing attempt
  // manager.
  requester1.ResetRequest();
  WaitForAttemptManagerComplete(requester1.associated_attempt_manager().get());
  ASSERT_FALSE(requester1.associated_attempt_manager());

  // The second request should succeed.
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));

  // Another reconnect should also succeed.
  Preconnector preconnector2(kDestination);
  EXPECT_THAT(preconnector2.Preconnect(pool()), IsOk());
}

TEST_F(HttpStreamPoolAttemptManagerTest, ResumeMultiplePausedJobs) {
  constexpr size_t kNumSuccessStreams = 3;

  // Add two fake DNS resolutions (one for failing case, another is for success
  // case).
  for (size_t i = 0; i < 2; ++i) {
    base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
        resolver()->AddFakeRequest();
    endpoint_request
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
  }

  auto failed_data = std::make_unique<SequencedSocketData>();
  failed_data->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(failed_data.get());

  HttpStreamKey stream_key = StreamKeyBuilder().Build();

  StreamRequester failing_requester(stream_key);
  failing_requester.RequestStream(pool());

  failing_requester.WaitForResult();
  EXPECT_THAT(failing_requester.result(),
              Optional(IsError(ERR_CONNECTION_RESET)));

  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  for (size_t i = 0; i < kNumSuccessStreams; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, OK));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));

    auto requester = std::make_unique<StreamRequester>(stream_key);
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());
    ASSERT_FALSE(raw_requester->result().has_value());
  }

  // Destroy the failed request. This should resume paused requests.
  failing_requester.ResetRequest();

  for (auto& requester : requesters) {
    requester->WaitForResult();
    EXPECT_THAT(requester->result(), Optional(IsOk()));
  }
}

TEST_F(HttpStreamPoolAttemptManagerTest, MultipleJobsFailAgain) {
  constexpr size_t kNumJobsAfterFailure = 3;

  // Add fake DNS resolutions since we will create at least three attempt
  // managers. +2 is for the first two failed attempts.
  for (size_t i = 0; i < kNumJobsAfterFailure + 2; ++i) {
    base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
        resolver()->AddFakeRequest();
    endpoint_request
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
  }

  auto failed_data1 = std::make_unique<SequencedSocketData>();
  failed_data1->set_connect_data(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(failed_data1.get());

  auto failed_data2 = std::make_unique<SequencedSocketData>();
  failed_data2->set_connect_data(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(failed_data1.get());

  HttpStreamKey stream_key = StreamKeyBuilder().Build();

  // The first request fails.
  StreamRequester failing_requester1(stream_key);
  failing_requester1.RequestStream(pool());
  Group* group = pool().GetGroupForTesting(stream_key);
  failing_requester1.WaitForResult();
  EXPECT_THAT(failing_requester1.result(),
              Optional(IsError(ERR_CONNECTION_RESET)));
  EXPECT_THAT(group->ShuttingDownAttemptManagerCount(), 1u);

  // The second request also fails.
  StreamRequester failing_requester2(stream_key);
  failing_requester2.RequestStream(pool());
  EXPECT_NE(failing_requester1.associated_attempt_manager().get(),
            failing_requester2.associated_attempt_manager().get());
  failing_requester2.WaitForResult();
  EXPECT_THAT(failing_requester2.result(),
              Optional(IsError(ERR_CONNECTION_RESET)));
  EXPECT_THAT(group->ShuttingDownAttemptManagerCount(), 2u);

  // Subsequent requests also fails.
  std::vector<std::unique_ptr<SequencedSocketData>> datas;
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  for (size_t i = 0; i < kNumJobsAfterFailure; ++i) {
    auto data = std::make_unique<SequencedSocketData>();
    data->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
    socket_factory()->AddSocketDataProvider(data.get());
    datas.emplace_back(std::move(data));

    auto requester = std::make_unique<StreamRequester>(stream_key);
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());
    ASSERT_FALSE(raw_requester->result().has_value());
  }

  // Destroy the first request. It should destroy the first AttemptManager.
  failing_requester1.ResetRequest();
  WaitForAttemptManagerComplete(
      failing_requester1.associated_attempt_manager().get());
  ASSERT_FALSE(failing_requester1.associated_attempt_manager());

  // Destroy the second request. It should destroy the second AttemptManager.
  failing_requester2.ResetRequest();
  WaitForAttemptManagerComplete(
      failing_requester2.associated_attempt_manager().get());
  ASSERT_FALSE(failing_requester2.associated_attempt_manager());

  // Complete subsequent requests.
  for (size_t i = 0; i < kNumJobsAfterFailure; ++i) {
    SCOPED_TRACE(i);
    requesters[i]->WaitForResult();
    EXPECT_THAT(requesters[i]->result(),
                Optional(IsError(ERR_CONNECTION_RESET)));
    requesters[i]->ResetRequest();
  }

  // Ensure the group is destroyed.
  // TODO(crbug.com/416088643): Add test callback to wait for Group's
  // completion.
  FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(pool().GetGroupForTesting(stream_key));
}

// Test that after a request fails and an SPDY session becomes available later,
// subsequent request/preconnect should succeed with the session.
TEST_F(HttpStreamPoolAttemptManagerTest, SpdySessionAvailableAfterFailure) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v6("2001:db8::1").endpoint())
      .CompleteStartSynchronously(OK);

  auto failed_data = std::make_unique<SequencedSocketData>();
  failed_data->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(failed_data.get());

  HttpStreamKey stream_key = StreamKeyBuilder(kDefaultDestination).Build();

  // The first request fails.
  StreamRequester failing_requester(stream_key);
  failing_requester.RequestStream(pool());
  Group* group = pool().GetGroupForTesting(stream_key);
  failing_requester.WaitForResult();
  EXPECT_THAT(failing_requester.result(),
              Optional(IsError(ERR_CONNECTION_RESET)));

  // Simulate creating a SpdySession before another request/preconnect.
  CreateFakeSpdySession(stream_key);

  // These request/preconnect use the existing SPDY session without
  // AttemptManager.
  StreamRequester requester(stream_key);
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());
  Preconnector preconnector(stream_key);
  // Preconnect succeeds immediately since there is an existing SPDY session.
  EXPECT_THAT(preconnector.Preconnect(pool()), IsOk());
  ASSERT_FALSE(group->attempt_manager());

  // Destroy the first request. It will destroy the first AttemptManager.
  failing_requester.ResetRequest();
  WaitForAttemptManagerComplete(
      failing_requester.associated_attempt_manager().get());
  ASSERT_FALSE(failing_requester.associated_attempt_manager());

  // Ensure the second request succeeds.
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoHTTP2);

  // Close the SPDY session so that the Group can complete. The Group should be
  // destroyed immediately because the second request completed without
  // AttemptManager so the Group can immediately complete.
  http_network_session()->CloseAllConnections(ERR_ABORTED, "For testing");
  FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(pool().GetGroupForTesting(stream_key));
}

// Test that after a request fails and an QUIC session becomes available later,
// subsequent request/preconnect should succeed with the session.
// This test uses an HTTP/3 Origin frame to make a session usable for
// the destination.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicSessionAvailableAfterFailure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v6("2001:db8::1").endpoint())
      .CompleteStartSynchronously(OK);

  auto failed_data = std::make_unique<SequencedSocketData>();
  failed_data->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(failed_data.get());

  HttpStreamKey stream_key = StreamKeyBuilder(kDefaultDestination).Build();

  // The first request fails.
  StreamRequester failing_requester(stream_key);
  failing_requester.RequestStream(pool());
  Group* group = pool().GetGroupForTesting(stream_key);
  failing_requester.WaitForResult();
  EXPECT_THAT(failing_requester.result(),
              Optional(IsError(ERR_CONNECTION_RESET)));

  // These request/preconnect uses a new AttemptManager as the previous
  // AttemptManager is failing.
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  StreamRequester requester(stream_key);
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());
  Preconnector preconnector(stream_key);
  preconnector.Preconnect(pool());
  ASSERT_FALSE(preconnector.result().has_value());
  ASSERT_NE(failing_requester.associated_attempt_manager().get(),
            group->attempt_manager());

  // Simulate creating a QUIC session that can be used for kDefaultDestination
  // before resuming the paused request/preconnect. The QUIC session is created
  // for kAltDestination and the session receives an HTTP/3 Origin frame that
  // indicates the session can be used for kDefaultDestination.
  {
    constexpr std::string_view kAltDestination = "https://alt.example.org";

    AddQuicData(/*host=*/kAltDestination);
    // Make the TCP attempt for kAltDestination stalled forever.
    SequencedSocketData tcp_alt;
    tcp_alt.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
    socket_factory()->AddSocketDataProvider(&tcp_alt);

    resolver()
        ->AddFakeRequest()
        ->add_endpoint(
            ServiceEndpointBuilder().add_v6("2001:db8::2").endpoint())
        .CompleteStartSynchronously(OK);

    StreamRequester alt_requester;
    alt_requester.set_destination(kAltDestination)
        .set_quic_version(quic_version())
        .RequestStream(pool());
    alt_requester.WaitForResult();
    EXPECT_THAT(alt_requester.result(), Optional(IsOk()));

    QuicSessionAliasKey alt_quic_key =
        alt_requester.GetStreamKey().CalculateQuicSessionAliasKey();
    QuicChromiumClientSession* alt_session =
        quic_session_pool()->FindExistingSession(alt_quic_key.session_key(),
                                                 alt_quic_key.destination());
    ASSERT_TRUE(alt_session);

    quic::OriginFrame origin_frame;
    origin_frame.origins.emplace_back(kDefaultDestination);
    alt_session->OnOriginFrame(origin_frame);
  }  // End of creating an existing QUIC session.

  // Finish DNS resolution to trigger checking existing QUIC session.
  // TODO(crbug.com/416364483): Ideally we should not depend on DNS resolution.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v6("2001:db8::1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoQUIC);
  preconnector.WaitForResult();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));

  // Destroy requests so that the group can complete.
  failing_requester.ResetRequest();
  requester.ResetRequest();
  WaitForAttemptManagerComplete(
      failing_requester.associated_attempt_manager().get());
  ASSERT_FALSE(pool().GetGroupForTesting(stream_key));
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReleaseStreamWhileFailing) {
  constexpr std::string_view kDestination = "http://a.test";

  SequencedSocketData data1;
  data1.set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(&data1);

  SequencedSocketData data2;
  data2.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  socket_factory()->AddSocketDataProvider(&data2);

  // Add two fake DNS resolutions (one for success case, another is for failure
  // case).
  for (size_t i = 0; i < 2; ++i) {
    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);
  }

  // Create an active HttpStream.
  StreamRequester requester1;
  const HttpStreamKey stream_key = requester1.GetStreamKey();
  requester1.set_destination(kDestination).RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  std::unique_ptr<HttpStream> stream1 = requester1.ReleaseStream();
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream1->RegisterRequest(&request_info);
  stream1->InitializeStream(/*can_send_early=*/false, RequestPriority::IDLE,
                            NetLogWithSource(), base::DoNothing());

  // Request the second stream. The request fails. The corresponding manager
  // becomes the failing state.
  StreamRequester requester2;
  requester2.set_destination(kDestination).RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsError(ERR_CONNECTION_REFUSED)));

  // Release the HttpStream. The manager should not do anything since it's
  // failing and requests are still alive.
  stream1.reset();

  // Reset the requests. The manager should complete.
  requester1.ResetRequest();
  requester2.ResetRequest();
  WaitForAttemptManagerComplete(requester1.associated_attempt_manager().get());
  ASSERT_FALSE(pool().GetOrCreateGroupForTesting(stream_key).attempt_manager());
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectPriority) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  auto data_a = std::make_unique<SequencedSocketData>();
  data_a->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data_a.get());

  Preconnector preconnector("https://a.test");
  int rv = preconnector.Preconnect(pool());
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
  EXPECT_EQ(pool()
                .GetOrCreateGroupForTesting(preconnector.GetStreamKey())
                .attempt_manager()
                ->GetPriority(),
            RequestPriority::IDLE);
}

// Tests that when an AttemptManager is failing, it's not treated as stalled.
TEST_F(HttpStreamPoolAttemptManagerTest, FailingIsNotStalled) {
  constexpr std::string_view kDestinationA = "http://a.test";
  constexpr std::string_view kDestinationB = "http://b.test";

  // For destination A. This fails.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  auto data_a = std::make_unique<SequencedSocketData>();
  data_a->set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(data_a.get());

  // For destination B. This succeeds.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);
  auto data_b = std::make_unique<SequencedSocketData>();
  data_b->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data_b.get());

  StreamRequester requester_a;
  requester_a.set_destination(kDestinationA).RequestStream(pool());
  requester_a.WaitForResult();
  EXPECT_THAT(requester_a.result(), Optional(IsError(ERR_CONNECTION_RESET)));

  StreamRequester requester_b;
  requester_b.set_destination(kDestinationB).RequestStream(pool());
  requester_b.WaitForResult();
  EXPECT_THAT(requester_b.result(), Optional(IsOk()));

  // Release the connection for B to try to process pending requests, but there
  // are no pending requests so it should do nothing.
  requester_b.ReleaseStream().reset();
}

// Tests that when an AttemptManager has a SPDY session, it's not treated as
// stalled.
TEST_F(HttpStreamPoolAttemptManagerTest, HavingSpdySessionIsNotStalled) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_factory()->AddSocketDataProvider(data.get());
  auto ssl = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
  ssl->next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(ssl.get());

  StreamRequester requester;
  requester.set_destination("https://a.test").RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  EXPECT_FALSE(pool()
                   .GetGroupForTesting(requester.GetStreamKey())
                   ->GetPriorityIfStalledByPoolLimit()
                   .has_value());
}

// Tests that when an AttemptManager has a QUIC session, it's not treated as
// stalled.
TEST_F(HttpStreamPoolAttemptManagerTest, HavingQuicSessionIsNotStalled) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // AttemptManager should already be destroyed.
  EXPECT_FALSE(
      pool().GetGroupForTesting(requester.GetStreamKey())->attempt_manager());
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReuseTypeUnused) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  StreamRequester requester;
  requester.RequestStream(pool());
  RunUntilIdle();
  ASSERT_THAT(requester.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  ASSERT_FALSE(stream->IsConnectionReused());
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReuseTypeUnusedIdle) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  // Preconnect to put an idle stream to the pool.
  Preconnector preconnector("http://a.test");
  preconnector.Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
  ASSERT_EQ(pool()
                .GetOrCreateGroupForTesting(preconnector.GetStreamKey())
                .IdleStreamSocketCount(),
            1u);

  StreamRequester requester;
  requester.RequestStream(pool());
  RunUntilIdle();
  ASSERT_THAT(requester.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  ASSERT_TRUE(stream->IsConnectionReused());
}

TEST_F(HttpStreamPoolAttemptManagerTest, ReuseTypeReusedIdle) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data = std::make_unique<SequencedSocketData>();
  data->set_connect_data(MockConnect(ASYNC, OK));
  socket_factory()->AddSocketDataProvider(data.get());

  StreamRequester requester1;
  requester1.RequestStream(pool());
  RunUntilIdle();
  ASSERT_THAT(requester1.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream1 = requester1.ReleaseStream();
  ASSERT_FALSE(stream1->IsConnectionReused());

  // Destroy the stream to make it an idle stream.
  stream1.reset();

  StreamRequester requester2;
  requester2.RequestStream(pool());
  RunUntilIdle();
  ASSERT_THAT(requester2.result(), Optional(IsOk()));
  std::unique_ptr<HttpStream> stream2 = requester2.ReleaseStream();
  ASSERT_TRUE(stream2->IsConnectionReused());
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicOk) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  // Set `is_quic_known_to_work_on_current_network` to false to check the flag
  // is updated to true after the QUIC attempt succeeds.
  quic_session_pool()->set_has_quic_ever_worked_on_current_network(false);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  AddQuicData();

  // Make TCP attempts stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Call both update and finish callbacks to make sure we don't attempt twice
  // for a single endpoint.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated()
      .CallOnServiceEndpointRequestFinished(OK);
  requester.WaitForResult();

  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoQUIC);
  EXPECT_TRUE(quic_session_pool()->has_quic_ever_worked_on_current_network());

  EXPECT_EQ(pool()
                .GetGroupForTesting(requester.GetStreamKey())
                ->ConnectingStreamSocketCount(),
            0u)
      << "Successful QUIC attempt should cancel in-flight TCP attempt";

  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  LoadTimingInfo timing_info;
  ASSERT_TRUE(stream->GetLoadTimingInfo(&timing_info));
  ValidateConnectTiming(timing_info.connect_timing);
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicOkSynchronouslyNoTcpAttempt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  AddQuicData();

  // No TCP data is needed because QUIC session attempt succeeds synchronously.

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());

  ASSERT_FALSE(
      pool().GetGroupForTesting(requester.GetStreamKey())->attempt_manager());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicOkDnsAlpn) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  AddQuicData();

  // Make TCP attempts stalled forever.
  SequencedSocketData tcp_data1;
  tcp_data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data1);
  SequencedSocketData tcp_data2;
  tcp_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data2);

  // Create two requests to make sure that one success QUIC session creation
  // completes all on-going requests.
  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination).RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination).RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder()
                         .add_v4("192.0.2.1")
                         .set_alpns({"h3", "h2"})
                         .endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_EQ(requester1.negotiated_protocol(), NextProto::kProtoQUIC);

  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_EQ(requester2.negotiated_protocol(), NextProto::kProtoQUIC);
}

// Regression test for crbug.com/403341337. QuicAttempt should not be started
// when the corresponding AttemptManager is failing.
TEST_F(HttpStreamPoolAttemptManagerTest, DontStartQuicAfterFailure) {
  AddQuicData();

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  // Request a stream to create an AttemptManager.
  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  Group* group = pool().GetGroupForTesting(requester.GetStreamKey());
  ASSERT_FALSE(requester.result().has_value());

  // Simulate a network change event to fail the AttemptManager. The
  // AttemptManager will reset ServiceEndpointRequest.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(endpoint_request);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
  ASSERT_FALSE(group->attempt_manager());
  ASSERT_EQ(group->ShuttingDownAttemptManagerCount(), 1u);

  // Ensure that the attempt manager completes after the request is destroyed.
  requester.ResetRequest();
  ASSERT_TRUE(requester.associated_attempt_manager().get());
  WaitForAttemptManagerComplete(requester.associated_attempt_manager().get());
}

// Tests that QUIC is not attempted when marked broken.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicBroken) {
  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "www.example.org", 443);
  http_server_properties()->MarkAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_NE(requester.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicFailBeforeTls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  MockConnectCompleter tls_completer;
  SequencedSocketData tls_data;
  tls_data.set_connect_data(MockConnect(&tls_completer));
  socket_factory()->AddSocketDataProvider(&tls_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  quic_completer.Complete(ERR_CONNECTION_REFUSED);
  // Fast forward to make QUIC attempt fail first.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(pool()
                  .GetOrCreateGroupForTesting(requester.GetStreamKey())
                  .attempt_manager()
                  ->GetQuicAttemptResultForTesting(),
              Optional(IsError(ERR_CONNECTION_REFUSED)));
  ASSERT_FALSE(requester.result().has_value());

  tls_completer.Complete(ERR_SOCKET_NOT_CONNECTED);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_SOCKET_NOT_CONNECTED)));

  // QUIC should not be marked as broken because TLS attempt also failed.
  const AlternativeService alternative_service(
      NextProto::kProtoQUIC,
      HostPortPair::FromSchemeHostPort(requester.GetStreamKey().destination()));
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicFailAfterTls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  MockConnectCompleter tls_completer;
  SequencedSocketData tls_data;
  tls_data.set_connect_data(MockConnect(&tls_completer));
  socket_factory()->AddSocketDataProvider(&tls_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  tls_completer.Complete(ERR_SOCKET_NOT_CONNECTED);
  // Fast forward to make TLS attempt fail first.
  FastForwardBy(base::Milliseconds(1));
  ASSERT_FALSE(requester.result().has_value());

  quic_completer.Complete(ERR_CONNECTION_REFUSED);
  requester.WaitForResult();
  EXPECT_THAT(
      requester.associated_attempt_manager()->GetQuicAttemptResultForTesting(),
      Optional(IsError(ERR_CONNECTION_REFUSED)));
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_CONNECTION_REFUSED)));

  // QUIC should not be marked as broken because TLS attempt also failed.
  const AlternativeService alternative_service(
      NextProto::kProtoQUIC,
      HostPortPair::FromSchemeHostPort(requester.GetStreamKey().destination()));
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicFailNoRemainingJobs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData tls_data;
  socket_factory()->AddSocketDataProvider(&tls_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // Release the stream and close the socket.
  requester.ResetRequest();
  requester.ReleaseStream().reset();
  pool()
      .GetGroupForTesting(requester.GetStreamKey())
      ->CloseIdleStreams("for testing");

  // The group should be alive since the QUIC attempt is ongoing.
  EXPECT_TRUE(pool().GetGroupForTesting(requester.GetStreamKey()));

  // Complete the QUIC attempt with an error. The group should be destroyed.
  quic_completer.Complete(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(pool().GetGroupForTesting(requester.GetStreamKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicFailNonBrokenErrors) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  const int kErrors[] = {ERR_NETWORK_CHANGED, ERR_INTERNET_DISCONNECTED};
  for (const int net_error : kErrors) {
    // Reset HttpServerProperties.
    InitializeSession();

    MockQuicData quic_data(quic_version());
    quic_data.AddConnect(ASYNC, net_error);
    quic_data.AddSocketDataToFactory(socket_factory());

    SequencedSocketData tcp_data;
    socket_factory()->AddSocketDataProvider(&tcp_data);
    SSLSocketDataProvider ssl(ASYNC, OK);
    socket_factory()->AddSSLSocketDataProvider(&ssl);

    resolver()
        ->AddFakeRequest()
        ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
        .CompleteStartSynchronously(OK);

    StreamRequester requester;
    requester.set_destination(kDefaultDestination)
        .set_quic_version(quic_version())
        .RequestStream(pool());
    requester.WaitForResult();
    EXPECT_THAT(requester.result(), Optional(IsOk()));
    EXPECT_NE(requester.negotiated_protocol(), NextProto::kProtoQUIC);

    // QUIC should not be marked as broken because QUIC attempt failed with
    // a protocol independent error.
    const AlternativeService alternative_service(
        NextProto::kProtoQUIC, HostPortPair::FromSchemeHostPort(
                                   requester.GetStreamKey().destination()));
    EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
        alternative_service, NetworkAnonymizationKey()))
        << ErrorToString(net_error);
  }
}

// Test that NetErrorDetails is populated when a QUIC session is created but
// it fails later.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicNetErrorDetails) {
  // QUIC attempt will pause. When resumed, it will fail.
  MockQuicData quic_data(quic_version());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData tls_data;
  tls_data.set_connect_data(MockConnect(ASYNC, ERR_SOCKET_NOT_CONNECTED));
  socket_factory()->AddSocketDataProvider(&tls_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  crypto_client_stream_factory()->set_handshake_mode(
      MockCryptoClientStream::COLD_START);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());

  // Fast forward to make TLS attempt fail first.
  FastForwardBy(base::Milliseconds(1));
  quic_data.Resume();
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_QUIC_PROTOCOL_ERROR)));
  EXPECT_EQ(requester.net_error_details().quic_connection_error,
            quic::QUIC_PACKET_READ_ERROR);
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicCanUseExistingSession) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  AddQuicData();

  // Make TCP attempts stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());

  // Invoke the update callback, run tasks, then invoke the finish callback to
  // make sure the finish callback checks the existing session.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_EQ(requester1.negotiated_protocol(), NextProto::kProtoQUIC);

  // The previous request created a session. This request should use the
  // existing session.
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_EQ(requester2.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, AlternativeSerivcesDisabled) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());
  requester.WaitForResult();

  EXPECT_THAT(requester.result(), Optional(IsOk()));
  ASSERT_FALSE(requester.associated_attempt_manager()
                   ->GetQuicAttemptResultForTesting()
                   .has_value());
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       AlternativeServicesDisabledThenEnabled) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  // Stall forever.
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  // Start a request that disables alternative services and cancel it
  // immediately.
  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());
  requester1.ResetRequest();

  AddQuicData();

  // Start another request that enables alternative services. It should complete
  // with a new QUIC session.
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_enable_alternative_services(true)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_EQ(requester2.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       AlternativeSerivcesDisabledQuicSessionExists) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);

  // Prerequisite: Create a QUIC session.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  AddQuicData();

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  requester1.ResetRequest();

  // Actual test: Request a stream without alternative services.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_NE(requester2.negotiated_protocol(), NextProto::kProtoQUIC);
}

// Tests that QUIC attempt fails when there is no known QUIC version and the
// DNS resolution indicates that the endpoint doesn't support QUIC.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicEndpointNotFoundNoDnsAlpn) {
  // Set that QUIC is working on the current network.
  quic_session_pool()->set_has_quic_ever_worked_on_current_network(true);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic::ParsedQuicVersion::Unsupported())
      .RequestStream(pool());
  requester.WaitForResult();

  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_THAT(
      requester.associated_attempt_manager()->GetQuicAttemptResultForTesting(),
      Optional(IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN)));
  // No matching ALPN should not update
  // `is_quic_known_to_work_on_current_network()`.
  EXPECT_TRUE(quic_session_pool()->has_quic_ever_worked_on_current_network());

  // QUIC should not be marked as broken.
  const AlternativeService alternative_service(
      NextProto::kProtoQUIC,
      HostPortPair::FromSchemeHostPort(requester.GetStreamKey().destination()));
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

// Tests that a QuicAttempt completes after finding an IP matching SPDY session.
TEST_F(HttpStreamPoolAttemptManagerTest, NoAlpnQuicAfterMatchingSpdySession) {
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);
  const HttpStreamKey alt_stream_key =
      StreamKeyBuilder("https://mail.example.org").Build();
  CreateFakeSpdySession(alt_stream_key, kCommonEndPoint);

  const HttpStreamKey stream_key =
      StreamKeyBuilder("https://www.example.org").Build();

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request->set_crypto_ready(true);

  // The first request triggers DNS resolution.
  StreamRequester requester1(stream_key);
  requester1.RequestStream(pool());
  ASSERT_FALSE(requester1.result().has_value());

  AttemptManager* manager =
      pool().GetGroupForTesting(stream_key)->attempt_manager();

  // The second request should not trigger a QUIC attempt in AttemptManager.
  StreamRequester requester2(stream_key);
  requester2.RequestStream(pool());
  ASSERT_FALSE(requester2.result().has_value());
  ASSERT_FALSE(manager->quic_attempt_for_testing());

  // Complete DNS resolution with an IP address that matches an existing SPDY
  // session.
  endpoint_request
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointRequestFinished(OK);

  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));

  // Ensure that AttemptManager doesn't attempt QUIC.
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(manager->quic_attempt_for_testing());
  EXPECT_FALSE(manager->GetQuicAttemptResultForTesting().has_value());
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicPreconnect) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  AddQuicData();

  SequencedSocketData tcp_data1;
  tcp_data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data1);
  SequencedSocketData tcp_data2;
  tcp_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data2);

  Preconnector preconnector1(kDefaultDestination);
  preconnector1.set_num_streams(2)
      .set_quic_version(quic_version())
      .Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector1.result(), Optional(IsOk()));

  // This preconnect request should complete immediately because we already have
  // an existing QUIC session.
  Preconnector preconnector2(kDefaultDestination);
  int rv = preconnector2.set_num_streams(1)
               .set_quic_version(quic_version())
               .Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(rv, IsOk());
}

// Tests that two destinations that resolve to the same IP address share the
// same QUIC session if allowed.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicMatchingIpSession) {
  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      resolver()->AddFakeRequest();
  endpoint_request1
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      resolver()->AddFakeRequest();
  endpoint_request2
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester2;
  requester2.set_destination(kAltDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  QuicSessionAliasKey quic_key1 =
      requester1.GetStreamKey().CalculateQuicSessionAliasKey();
  QuicSessionAliasKey quic_key2 =
      requester2.GetStreamKey().CalculateQuicSessionAliasKey();
  ASSERT_EQ(quic_session_pool()->FindExistingSession(quic_key1.session_key(),
                                                     quic_key1.destination()),
            quic_session_pool()->FindExistingSession(quic_key2.session_key(),
                                                     quic_key2.destination()));
}

// Regression test for crrev.com/c/405361789. There could be a matching QUIC
// session while attempting a new QUIC session as a result of receiving an
// HTTP/3 Origin frame. In such case, a preconnect request should succeed
// with the matching QUIC session.
TEST_F(HttpStreamPoolAttemptManagerTest, H3OriginFrameWhileAttemptingQuic) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  // Step1: Request a stream to create a QUIC session for kDefaultDestination.

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data1;
  tcp_data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data1);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      resolver()->AddFakeRequest();
  endpoint_request1
      ->add_endpoint(ServiceEndpointBuilder().add_v6("2001:db8::1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // Step2: Request a preconnect to initiate another QUIC session attempt for
  // kAltDestination. The second session will be closed later.

  constexpr std::string_view kAltDestination = "https://alt.example.org";

  MockConnectCompleter quic_completer;
  AddQuicData(/*host=*/kAltDestination, &quic_completer,
              quic::QUIC_CONNECTION_IP_POOLED,
              "An active session exists for the given IP.");

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data2;
  tcp_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data2);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      resolver()->AddFakeRequest();
  endpoint_request2
      ->add_endpoint(ServiceEndpointBuilder().add_v6("2001:db8::2").endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector(kAltDestination);
  preconnector.set_quic_version(quic_version()).Preconnect(pool());

  // Step3: Simulate receiving HTTP/3 Origin frame that contains kAltDestination
  // as an origin.

  QuicSessionAliasKey quic_key1 =
      requester.GetStreamKey().CalculateQuicSessionAliasKey();
  QuicChromiumClientSession* quic_session1 =
      quic_session_pool()->FindExistingSession(quic_key1.session_key(),
                                               quic_key1.destination());
  ASSERT_TRUE(quic_session1);

  quic::OriginFrame origin_frame;
  origin_frame.origins.emplace_back(kAltDestination);
  quic_session1->OnOriginFrame(origin_frame);

  // Step4: Ensure that processing pending requests doesn't cause any problems.

  pool().ProcessPendingRequestsInGroups();

  // Step5: Ensure that the preconnect request completes with the matching QUIC
  // session.

  quic_completer.Complete(OK);
  preconnector.WaitForResult();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));

  QuicSessionAliasKey quic_key2 =
      preconnector.GetStreamKey().CalculateQuicSessionAliasKey();
  QuicChromiumClientSession* quic_session2 =
      quic_session_pool()->FindExistingSession(quic_key2.session_key(),
                                               quic_key2.destination());
  ASSERT_TRUE(quic_session1);
  ASSERT_EQ(quic_session1, quic_session2);
}

// The same as above test, but the ServiceEndpointRequest provides two IP
// addresses separately, the first address does not match the existing session
// and the second address matches the existing session.
TEST_F(HttpStreamPoolAttemptManagerTest,
       QuicMatchingIpSessionOnEndpointsUpdated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  // Make the second QUIC attempt stalled forever.
  SequencedSocketData quic_data2;
  quic_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&quic_data2);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      resolver()->AddFakeRequest();
  endpoint_request1
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      resolver()->AddFakeRequest();

  StreamRequester requester2;
  requester2.set_destination(kAltDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  endpoint_request2
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester2.result().has_value());

  endpoint_request2
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointsUpdated();
  RunUntilIdle();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  QuicSessionAliasKey quic_key1 =
      requester1.GetStreamKey().CalculateQuicSessionAliasKey();
  QuicSessionAliasKey quic_key2 =
      requester2.GetStreamKey().CalculateQuicSessionAliasKey();
  EXPECT_EQ(quic_session_pool()->FindExistingSession(quic_key1.session_key(),
                                                     quic_key1.destination()),
            quic_session_pool()->FindExistingSession(quic_key2.session_key(),
                                                     quic_key2.destination()));
}

// Tests that preconnect completes when there is a QUIC session of which IP
// address matches to the service endpoint resolution of the preconnect.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicPreconnectMatchingIpSession) {
  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  AddQuicData();

  // Make the TCP attempt stalled forever.
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      resolver()->AddFakeRequest();
  endpoint_request1
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      resolver()->AddFakeRequest();
  endpoint_request2
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector2(kAltDestination);
  preconnector2.set_quic_version(quic_version()).Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector2.result(), Optional(IsOk()));
  QuicSessionAliasKey quic_key1 =
      requester1.GetStreamKey().CalculateQuicSessionAliasKey();
  QuicSessionAliasKey quic_key2 =
      preconnector2.GetStreamKey().CalculateQuicSessionAliasKey();
  EXPECT_EQ(quic_session_pool()->FindExistingSession(quic_key1.session_key(),
                                                     quic_key1.destination()),
            quic_session_pool()->FindExistingSession(quic_key2.session_key(),
                                                     quic_key2.destination()));
}

// Tests that when disabled IP-based pooling, QUIC attempts are also disabled.
// TODO(crbug.com/346835898): Make sure this behavior is what we actually want.
// In production code, we currently disable both IP-based pooling and QUIC at
// the same time.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicMatchingIpSessionDisabled) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_enable_ip_based_pooling(false)
      .RequestStream(pool());
  RunUntilIdle();

  EXPECT_THAT(requester.result(), Optional(IsOk()));
  ASSERT_FALSE(requester.associated_attempt_manager()
                   ->GetQuicAttemptResultForTesting()
                   .has_value());
}

TEST_F(HttpStreamPoolAttemptManagerTest, DelayStreamAttemptQuicOk) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  AddQuicData();

  // Don't add any TCP data. This makes sure that the following request
  // completes with a QUIC session without attempting TCP-based protocols.

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DelayStreamAttemptQuicFail) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data->AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // QUIC should be marked as broken.
  const AlternativeService alternative_service(
      NextProto::kProtoQUIC,
      HostPortPair::FromSchemeHostPort(requester.GetStreamKey().destination()));
  EXPECT_TRUE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

// Test the behavior of the stream attempt delay timer with
// StreamAttemptDelayBehavior::kStartTimerOnFirstEndpointUpdate.
TEST_F(HttpStreamPoolAttemptManagerTest,
       StreamAttemptDelayPassedTimerStartOnFirstEndpointUpdate) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(10);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kAsyncQuicSession, {}},
       {features::kHappyEyeballsV3,
        {{HttpStreamPool::kTcpBasedAttemptDelayBehaviorParamName.data(),
          HttpStreamPool::kTcpBasedAttemptDelayBehaviorOptions[0].name}}}},
      /*disabled_features=*/{});

  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  // QUIC attempt stalls forever.
  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data->AddSocketDataToFactory(socket_factory());

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .attempt_manager();

  // Provide an IP address. QUIC attempt isn't triggered yet since it's not
  // ready for cryptographic handshakes, but the stream attempt delay timer
  // is triggered.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester.result().has_value());
  ASSERT_FALSE(manager->quic_attempt_for_testing());

  // Complete service endpoint resolution with the delay. Trigger both the QUIC
  // task and a TCP-based attempt.
  FastForwardBy(kDelay);
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  ASSERT_FALSE(requester.result().has_value());
  ASSERT_TRUE(manager->quic_attempt_for_testing());
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 1u);

  // The request should complete with the TCP-based attempt.
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Test the behavior of the stream attempt delay timer with
// StreamAttemptDelayBehavior::kStartTimerOnFirstQuicAttempt.
TEST_F(HttpStreamPoolAttemptManagerTest,
       StreamAttemptDelayPassedTimerStartOnFirstQuicAttempt) {
  constexpr base::TimeDelta kQuicDelay = base::Milliseconds(10);
  constexpr base::TimeDelta kDnsDelay = base::Milliseconds(40);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kAsyncQuicSession, {}},
       {features::kHappyEyeballsV3,
        {{HttpStreamPool::kTcpBasedAttemptDelayBehaviorParamName.data(),
          HttpStreamPool::kTcpBasedAttemptDelayBehaviorOptions[1].name}}}},
      /*disabled_features=*/{});

  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kQuicDelay);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  // QUIC attempt stalls forever.
  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data->AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .attempt_manager();

  // Provide an IP address. QUIC attempt isn't triggered yet since it's not
  // ready for cryptographic handshakes.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester.result().has_value());
  ASSERT_FALSE(manager->quic_attempt_for_testing());

  // Complete service endpoint resolution with a delay. Trigger a QUIC attempt,
  // but TCP-based attempt should not be triggered yet.
  FastForwardBy(kDnsDelay);
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  ASSERT_FALSE(requester.result().has_value());
  ASSERT_TRUE(manager->quic_attempt_for_testing());
  ASSERT_EQ(manager->TcpBasedAttemptCount(), 0u);

  // Fire the stream attempt delay timer. The request should complete.
  FastForwardBy(kQuicDelay);
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Test the behavior of the stream attempt delay timer with
// StreamAttemptDelayBehavior::kStartTimerOnFirstQuicAttempt.
// A preconnect should complete with a TCP connection when there is no known
// QUIC version and DNS resolution indicates that the endpoint doesn't support
// QUIC.
TEST_F(HttpStreamPoolAttemptManagerTest,
       StreamAttemptDelayPassedForPreconnectTimerStartOnFirstQuicAttempt) {
  constexpr base::TimeDelta kDelay = base::Milliseconds(40);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kAsyncQuicSession, {}},
       {features::kHappyEyeballsV3,
        {{HttpStreamPool::kTcpBasedAttemptDelayBehaviorParamName.data(),
          HttpStreamPool::kTcpBasedAttemptDelayBehaviorOptions[1].name}}}},
      /*disabled_features=*/{});

  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  MockConnectCompleter connect_completer;
  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(&connect_completer));
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  Preconnector preconnector(kDefaultDestination);
  preconnector.Preconnect(pool());
  ASSERT_FALSE(preconnector.result().has_value());

  AttemptManager* manager =
      pool()
          .GetOrCreateGroupForTesting(preconnector.GetStreamKey())
          .attempt_manager();

  // Provide an IP address. QUIC attempt isn't triggered yet since it's not
  // ready for cryptographic handshakes.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(preconnector.result().has_value());
  ASSERT_FALSE(manager->quic_attempt_for_testing());

  // Complete service endpoint resolution with a delay. The QuicAttempt should
  // complete with an error and a TCP-based attempt should be triggered.
  FastForwardBy(kDelay);
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  // QuicAttempt uses PostTask to notify the error
  // (ERR_DNS_NO_MATCHING_SUPPORTED_ALPN). This FastForwardBy() is needed to run
  // the task. Note that FastForwardUntilNoTasksRemain() doesn't work since it
  // causes a connection timeout.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(preconnector.result().has_value());
  EXPECT_FALSE(manager->quic_attempt_for_testing());
  EXPECT_THAT(manager->GetQuicAttemptResultForTesting(),
              Optional(IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN)));
  EXPECT_EQ(manager->TcpBasedAttemptCount(), 1u);

  connect_completer.Complete(OK);
  preconnector.WaitForResult();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       DelayStreamAttemptDisableAlternativeServicesLater) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  constexpr base::TimeDelta kDelay = base::Milliseconds(10);
  quic_session_pool()->SetTimeDelayForWaitingJobForTesting(kDelay);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data->AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointRequestFinished(OK);
  RunUntilIdle();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnOk) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  AddQuicData();

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnExistingSession) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  AddQuicData();

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // The first request. It should create a QUIC session.
  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination).RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));
  EXPECT_EQ(requester1.negotiated_protocol(), NextProto::kProtoQUIC);

  // The second request. The request disables alternative services but the
  // QUIC session should be used because QUIC is forced by the
  // HttpNetworkSession. If the second request doesn't use the existing session
  // this test fails because we call AddQuicData() only once so we only added
  // mock reads and writes for only one QUIC connection.
  StreamRequester requester2;
  requester2.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());
  requester2.WaitForResult();
  EXPECT_THAT(requester2.result(), Optional(IsOk()));
  EXPECT_EQ(requester2.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnFail) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data->AddSocketDataToFactory(socket_factory());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());
  RunUntilIdle();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_CONNECTION_REFUSED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnPreconnectOk) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  AddQuicData();

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector(kDefaultDestination);
  preconnector.Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, OriginsToForceQuicOnPreconnectFail) {
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  auto quic_data = std::make_unique<MockQuicData>(quic_version());
  quic_data->AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data->AddSocketDataToFactory(socket_factory());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  Preconnector preconnector(kDefaultDestination);
  preconnector.Preconnect(pool());
  RunUntilIdle();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_CONNECTION_REFUSED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, QuicSessionGoneBeforeUsing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);
  origins_to_force_quic_on().insert(
      HostPortPair::FromURL(GURL(kDefaultDestination)));
  InitializeSession();

  QuicTestPacketMaker* client_maker = CreateQuicClientPacketMaker();
  MockQuicData quic_data(quic_version());
  quic_data.AddWrite(SYNCHRONOUS, client_maker->MakeInitialSettingsPacket(
                                      /*packet_number=*/1));
  quic_data.AddRead(ASYNC, ERR_SOCKET_NOT_CONNECTED);
  quic_data.AddSocketDataToFactory(socket_factory());

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // QUIC attempt succeeds since we didn't require confirmation.
  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // Try to initialize `stream`. The underlying socket was already closed so
  // the initialization fails.
  std::unique_ptr<HttpStream> stream = requester.ReleaseStream();
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  stream->RegisterRequest(&request_info);
  int rv =
      stream->InitializeStream(/*can_send_early=*/false, RequestPriority::IDLE,
                               NetLogWithSource(), base::DoNothing());
  EXPECT_THAT(rv, IsError(ERR_CONNECTION_CLOSED));
}

TEST_F(HttpStreamPoolAttemptManagerTest, GetInfoAsValue) {
  // Add an idle stream to a.test and create an in-flight connection attempt for
  // b.test.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester_a.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data_b = std::make_unique<SequencedSocketData>();
  data_b->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data_b.get());

  requester_b.RequestStream(pool());

  base::Value::Dict info = pool().GetInfoAsValue();
  EXPECT_THAT(info.FindInt("idle_socket_count"), Optional(1));
  EXPECT_THAT(info.FindInt("connecting_socket_count"), Optional(1));
  EXPECT_THAT(info.FindInt("max_socket_count"),
              Optional(pool().max_stream_sockets_per_pool()));
  EXPECT_THAT(info.FindInt("max_sockets_per_group"),
              Optional(pool().max_stream_sockets_per_group()));

  base::Value::Dict* groups_info = info.FindDict("groups");
  ASSERT_TRUE(groups_info);

  base::Value::Dict* info_a =
      groups_info->FindDict(requester_a.GetStreamKey().ToString());
  ASSERT_TRUE(info_a);
  EXPECT_THAT(info_a->FindInt("active_socket_count"), Optional(1));
  EXPECT_THAT(info_a->FindInt("idle_socket_count"), Optional(1));

  base::Value::Dict* info_b =
      groups_info->FindDict(requester_b.GetStreamKey().ToString());
  ASSERT_TRUE(info_b);
  EXPECT_THAT(info_b->FindInt("active_socket_count"), Optional(1));
  EXPECT_THAT(info_b->FindInt("idle_socket_count"), Optional(0));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcH2OkOriginFail) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. Negotiate HTTP/2 with the alternative service.
  const MockRead alt_reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockWrite alt_writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  SequencedSocketData alt_data(alt_reads, alt_writes);
  socket_factory()->AddSocketDataProvider(&alt_data);
  SSLSocketDataProvider alt_ssl(ASYNC, OK);
  alt_ssl.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&alt_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. The connection is refused.
  StaticSocketDataProvider origin_data;
  origin_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  socket_factory()->AddSocketDataProvider(&origin_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  requester.ResetRequest();
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcFailOriginOk) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. The connection is reset.
  StaticSocketDataProvider alt_data;
  alt_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_RESET));
  socket_factory()->AddSocketDataProvider(&alt_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. Negotiated HTTP/1.1 with the origin.
  StaticSocketDataProvider origin_data;
  socket_factory()->AddSocketDataProvider(&origin_data);
  SSLSocketDataProvider origin_ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&origin_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  requester.ResetRequest();
  EXPECT_TRUE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcNegotiatedWithH1) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. Negotiated with HTTP/1.1.
  StaticSocketDataProvider alt_data;
  socket_factory()->AddSocketDataProvider(&alt_data);
  SSLSocketDataProvider alt_ssl(ASYNC, OK);
  alt_ssl.next_proto = NextProto::kProtoHTTP11;
  socket_factory()->AddSSLSocketDataProvider(&alt_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. The connection is refused.
  StaticSocketDataProvider origin_data;
  origin_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  socket_factory()->AddSocketDataProvider(&origin_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(),
              Optional(IsError(ERR_ALPN_NEGOTIATION_FAILED)));
  requester.ResetRequest();
  // Both the origin and alternavie service failed, so the alternative service
  // should not be marked broken.
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcCertificateError) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. Certificate is invalid.
  StaticSocketDataProvider alt_data;
  socket_factory()->AddSocketDataProvider(&alt_data);
  SSLSocketDataProvider alt_ssl(ASYNC, ERR_CERT_DATE_INVALID);
  alt_ssl.next_proto = NextProto::kProtoHTTP11;
  socket_factory()->AddSSLSocketDataProvider(&alt_ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. The connection is stalled forever.
  StaticSocketDataProvider origin_data;
  origin_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&origin_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_CERT_DATE_INVALID)));
  requester.ResetRequest();
  // The alternavie service failed and origin didn't complete, so the
  // alternative service should not be marked broken.
  EXPECT_FALSE(http_server_properties()->IsAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcSetPriority) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoHTTP2,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateHttp2AlternativeServiceInfo(
          alternative_service, expiration));

  // For the alternative service. The connection is stalled forever.
  StaticSocketDataProvider alt_data;
  alt_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&alt_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For the origin. The connection is stalled forever.
  StaticSocketDataProvider origin_data;
  origin_data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&origin_data);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  HttpStreamRequest* request =
      requester.set_priority(RequestPriority::LOW).RequestStream(pool());

  AttemptManager* origin_manager =
      pool()
          .GetOrCreateGroupForTesting(requester.GetStreamKey())
          .attempt_manager();
  ASSERT_TRUE(origin_manager);
  EXPECT_EQ(origin_manager->GetPriority(), RequestPriority::LOW);

  HttpStreamKey alt_stream_key =
      StreamKeyBuilder()
          .set_destination(url::SchemeHostPort(
              url::kHttpsScheme, kAlternative.host(), kAlternative.port()))
          .Build();
  AttemptManager* alt_manager =
      pool().GetOrCreateGroupForTesting(alt_stream_key).attempt_manager();
  ASSERT_TRUE(alt_manager);
  EXPECT_EQ(alt_manager->GetPriority(), RequestPriority::LOW);

  request->SetPriority(RequestPriority::HIGHEST);
  EXPECT_EQ(origin_manager->GetPriority(), RequestPriority::HIGHEST);
  EXPECT_EQ(alt_manager->GetPriority(), RequestPriority::HIGHEST);
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcQuicOk) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoQUIC,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service, expiration, DefaultSupportedQuicVersions()));

  AddQuicData();

  // For QUIC alt-svc.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For origin. Endpoint resolution never completes.
  resolver()->AddFakeRequest();

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcQuicFailOriginOk) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoQUIC,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service, expiration, DefaultSupportedQuicVersions()));

  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  // For QUIC alt-svc.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For origin.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_NE(requester.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcQuicFailOriginFail) {
  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoQUIC,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  StreamRequester requester;
  requester.set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service, expiration, DefaultSupportedQuicVersions()));

  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_FAILED));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  // For QUIC alt-svc.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For origin.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.2").endpoint())
      .CompleteStartSynchronously(OK);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_CONNECTION_FAILED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, AltSvcQuicUseExistingSession) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  const url::SchemeHostPort kOrigin(url::kHttpsScheme, "origin.example.org",
                                    443);
  const HostPortPair kAlternative("alt.example.org", 443);

  const AlternativeService alternative_service(NextProto::kProtoQUIC,
                                               kAlternative);
  const base::Time expiration = base::Time::Now() + base::Days(1);

  // The first request creates a QUIC session.
  auto requester1 = std::make_unique<StreamRequester>();
  requester1->set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service, expiration, DefaultSupportedQuicVersions()));

  AddQuicData();

  // For QUIC alt-svc.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // For origin. Endpoint resolution never completes.
  resolver()->AddFakeRequest();

  requester1->RequestStream(pool());
  requester1->WaitForResult();
  EXPECT_THAT(requester1->result(), Optional(IsOk()));
  EXPECT_EQ(requester1->negotiated_protocol(), NextProto::kProtoQUIC);

  requester1.reset();

  // The second request uses the existing QUIC session.
  auto requester2 = std::make_unique<StreamRequester>();
  requester2->set_destination(kOrigin).set_alternative_service_info(
      AlternativeServiceInfo::CreateQuicAlternativeServiceInfo(
          alternative_service, expiration, DefaultSupportedQuicVersions()));

  requester2->RequestStream(pool());
  requester2->WaitForResult();
  EXPECT_THAT(requester2->result(), Optional(IsOk()));
  EXPECT_EQ(requester2->negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, FlushWithError) {
  // Add an idle stream to a.test and create an in-flight connection attempt for
  // b.test.
  StreamRequester requester_a;
  requester_a.set_destination("https://a.test");
  Group& group = pool().GetOrCreateGroupForTesting(requester_a.GetStreamKey());
  group.AddIdleStreamSocket(std::make_unique<FakeStreamSocket>());

  StreamRequester requester_b;
  requester_b.set_destination("https://b.test");

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  auto data_b = std::make_unique<SequencedSocketData>();
  data_b->set_connect_data(MockConnect(ASYNC, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(data_b.get());

  requester_b.RequestStream(pool());

  // At this point, there are 2 active streams (one is idle and the other is
  // in-flight).
  EXPECT_EQ(pool().TotalActiveStreamCount(), 2u);

  // Flushing should destroy all active streams and in-flight attempts.
  pool().FlushWithError(ERR_ABORTED, StreamSocketCloseReason::kUnspecified,
                        "For testing");
  EXPECT_EQ(pool().TotalActiveStreamCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, FlushWithErrorPendingJobs) {
  constexpr size_t kNumRequestsAfterFailure = 3;
  const HttpStreamKey stream_key = StreamKeyBuilder().Build();

  // (Preparation) The first request fails. The group enters the failing mode.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  SequencedSocketData failed_data;
  failed_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  socket_factory()->AddSocketDataProvider(&failed_data);

  StreamRequester failing_requester(stream_key);
  failing_requester.RequestStream(pool());
  Group* group = pool().GetGroupForTesting(stream_key);
  failing_requester.WaitForResult();
  EXPECT_THAT(failing_requester.result(),
              Optional(IsError(ERR_CONNECTION_REFUSED)));
  EXPECT_FALSE(group->attempt_manager());
  EXPECT_TRUE(
      failing_requester.associated_attempt_manager()->is_shutting_down());

  // Subsequent requests (jobs) uses a new AttemptManager. Thsese requests are
  // blocked by DNS resolution.
  resolver()->AddFakeRequest();
  std::vector<std::unique_ptr<StreamRequester>> requesters;
  for (size_t i = 0; i < kNumRequestsAfterFailure; ++i) {
    auto requester = std::make_unique<StreamRequester>(stream_key);
    StreamRequester* raw_requester = requester.get();
    requesters.emplace_back(std::move(requester));
    raw_requester->RequestStream(pool());
    ASSERT_FALSE(raw_requester->result().has_value());
  }
  base::WeakPtr<AttemptManager> second_attempt_manager =
      group->attempt_manager()->GetWeakPtrForTesting();
  EXPECT_NE(failing_requester.associated_attempt_manager().get(),
            second_attempt_manager.get());

  // Abort requests. The second AttemptManager also fails.
  pool().FlushWithError(ERR_ABORTED, StreamSocketCloseReason::kUnspecified,
                        "for testing");
  for (auto& requester : requesters) {
    requester->WaitForResult();
    EXPECT_THAT(requester->result(), Optional(IsError(ERR_ABORTED)));
  }
  EXPECT_TRUE(second_attempt_manager->is_shutting_down());

  // Destroy the first request. This should result in attempting to delete the
  // group. The group should be still alive since we don't destroy all requests
  // yet.
  failing_requester.ResetRequest();
  EXPECT_TRUE(pool().GetGroupForTesting(stream_key));

  // Destroy all requests. The group should be deleted.
  for (auto& requester : requesters) {
    requester->ResetRequest();
  }

  // Ensure the group is destroyed. Waiting for completion of one failing
  // AttemptManager is sufficient to destroy the group.
  WaitForAttemptManagerComplete(
      failing_requester.associated_attempt_manager().get());
  ASSERT_FALSE(failing_requester.associated_attempt_manager().get());
  ASSERT_FALSE(second_attempt_manager.get());
  EXPECT_FALSE(pool().GetGroupForTesting(stream_key));
  EXPECT_EQ(pool().TotalActiveStreamCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, UnsafePort) {
  StreamRequester requester;
  requester.set_destination("http://www.example.org:7");

  const url::SchemeHostPort destination =
      requester.GetStreamKey().destination();
  ASSERT_FALSE(
      IsPortAllowedForScheme(destination.port(), destination.scheme()));

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_UNSAFE_PORT)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, PreconnectUnsafePort) {
  Preconnector preconnector("http://www.example.org:7");

  const url::SchemeHostPort destination =
      preconnector.GetStreamKey().destination();
  ASSERT_FALSE(
      IsPortAllowedForScheme(destination.port(), destination.scheme()));

  preconnector.Preconnect(pool());
  preconnector.WaitForResult();
  EXPECT_THAT(preconnector.result(), Optional(IsError(ERR_UNSAFE_PORT)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DisallowH1) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // Nagotiate to use HTTP/1.1.
  StaticSocketDataProvider data;
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.set_is_http1_allowed(false);

  requester.RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_H2_OR_QUIC_REQUIRED)));
}

// Tests that a bad proxy is reported to a ProxyResolutionService when falling
// back to the direct connection succeeds.
TEST_F(HttpStreamPoolAttemptManagerTest, ReportBadProxyAfterSuccessOnDirect) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  StaticSocketDataProvider data;
  socket_factory()->AddSocketDataProvider(&data);

  // Simulate that we have a bad proxy.
  ProxyInfo proxy_info;
  proxy_info.UsePacString("PROXY badproxy:80; DIRECT");
  proxy_info.Fallback(ERR_PROXY_CONNECTION_FAILED, NetLogWithSource());

  StreamRequester requester;
  requester.set_proxy_info(proxy_info).RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));

  // The ProxyResolutionService should know that the proxy is bad.
  auto proxy_chain = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_HTTP, "badproxy", 80);
  const ProxyRetryInfoMap retry_info_map =
      http_network_session()->proxy_resolution_service()->proxy_retry_info();
  auto it = retry_info_map.find(proxy_chain);
  ASSERT_TRUE(it != retry_info_map.end());
  EXPECT_THAT(it->second.net_error, IsError(ERR_PROXY_CONNECTION_FAILED));
}

TEST_F(HttpStreamPoolAttemptManagerTest, DirectProxyInfoForIpProtection) {
  const auto kIpProtectionDirectChain =
      ProxyChain::ForIpProtection(std::vector<ProxyServer>());
  ProxyInfo proxy_info;
  proxy_info.UseProxyChain(kIpProtectionDirectChain);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  StaticSocketDataProvider data;
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.set_proxy_info(proxy_info).RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.used_proxy_info().ToDebugString(),
            proxy_info.ToDebugString());
}

// Regression test for crbug.com/369744951. Ensures that destroying
// an HttpNetworkSession, which owns an HttpStreamPool, doesn't cause a crash
// when a StreamSocket is returned to the pool in the middle of the destruction.
TEST_F(HttpStreamPoolAttemptManagerTest,
       DestroyHttpNetworkSessionWithSpdySession) {
  // Add a SpdySession. The session will be destroyed when the
  // HttpNetworkSession is being destroyed. The underlying StreamSocket will be
  // released to HttpStreamPool::Group.
  CreateFakeSpdySession(
      StreamKeyBuilder().set_destination("https://a.test").Build());

  // Create a request to a different destination. The request never finishes.
  StreamRequester requester;
  requester.set_destination("https://b.test");
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);
  StaticSocketDataProvider data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&data);
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Cancel the request before destructing HttpNetworkSession to avoid a
  // dangling pointer.
  requester.ResetRequest();

  // Destroying HttpNetworkSession should not cause crash.
  DestroyHttpNetworkSession();
}

// Regression test for crbug.com/371894055.
TEST_F(HttpStreamPoolAttemptManagerTest,
       AsyncQuicSessionDestroyRequestBeforeSessionCreation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  constexpr std::string_view kAltDestination = "https://alt.example.org";
  const IPEndPoint kCommonEndPoint = MakeIPEndPoint("2001:db8::1", 443);

  // Precondition: Create a QUIC session that can be shared for destinations
  // that are resolved to kCommonEndPoint.
  AddQuicData();

  SequencedSocketData tcp_data1;
  tcp_data1.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data1);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester1;
  requester1.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester1.WaitForResult();
  EXPECT_THAT(requester1.result(), Optional(IsOk()));

  // Actual test: Create a request that starts a QuicSessionAttempt, which
  // is later destroyed since there is a matching IP session.

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData tcp_data2;
  tcp_data2.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&tcp_data2);

  StreamRequester requester2;
  requester2.set_destination(kAltDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester2.result().has_value());

  // Provide a different IP address to start a QUIC attempt.
  endpoint_request->set_crypto_ready(true)
      .add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CallOnServiceEndpointsUpdated();
  ASSERT_FALSE(requester2.result().has_value());

  // Provide kCommonEndPoint so that the corresponding attempt manager cancel
  // the in-flight QUIC attempt and use the existing session.
  endpoint_request->set_crypto_ready(true)
      .add_endpoint(
          ServiceEndpointBuilder().add_ip_endpoint(kCommonEndPoint).endpoint())
      .CallOnServiceEndpointsUpdated();

  // Resume the QUIC attempt. This should not detect a dangling pointer.
  quic_completer.Complete(OK);
  requester2.WaitForResult();
}

TEST_F(HttpStreamPoolAttemptManagerTest, EchOk) {
  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("www.example.org", /*max_name_len=*/128,
                              &ech_config_list));

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expected_ech_config_list = ech_config_list;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder()
                         .add_v4("192.0.2.1")
                         .set_alpns({"http/1.1"})
                         .set_ech_config_list(ech_config_list)
                         .endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, EchDisabled) {
  SetEchEnabled(false);

  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("www.example.org", /*max_name_len=*/128,
                              &ech_config_list));

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  // ECH config list should not be set since ECH is disabled.
  ssl.expected_ech_config_list = std::vector<uint8_t>();
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder()
                         .add_v4("192.0.2.1")
                         .set_alpns({"http/1.1"})
                         .set_ech_config_list(ech_config_list)
                         .endpoint())
      .CompleteStartSynchronously(OK);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, EchSvcbOptional) {
  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("www.example.org", /*max_name_len=*/128,
                              &ech_config_list));

  // The first endpoint provides ECH config list. The second endpoint doesn't.
  // This makes attempts SVCB-optional.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder()
                         .add_v4("192.0.2.1")
                         .set_alpns({"http/1.1"})
                         .set_ech_config_list(ech_config_list)
                         .endpoint())
      .add_endpoint(ServiceEndpointBuilder()
                        .add_v4("192.0.2.2")
                        .set_alpns({"http/1.1"})
                        .endpoint())
      .CompleteStartSynchronously(OK);

  // The first endpoint fails.
  SequencedSocketData data1;
  data1.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_FAILED));
  socket_factory()->AddSocketDataProvider(&data1);

  // The second endpoint succeeds.
  SequencedSocketData data2;
  socket_factory()->AddSocketDataProvider(&data2);
  SSLSocketDataProvider ssl2(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl2);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest, EchSvcbReliant) {
  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("www.example.org", /*max_name_len=*/128,
                              &ech_config_list));

  // All endpoints have ECH config list. The first endpoint only accepts H3. The
  // second endpoint only accepts HTTP/1.1. This makes attempts SVCB-relient.
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder()
                         .add_v4("192.0.2.1")
                         .set_alpns({"h3"})
                         .set_ech_config_list(ech_config_list)
                         .endpoint())
      .add_endpoint(ServiceEndpointBuilder()
                        .add_v4("192.0.2.2")
                        .set_alpns({"http/1.1"})
                        .set_ech_config_list(ech_config_list)
                        .endpoint())
      .CompleteStartSynchronously(OK);

  // The first endpoint (H3) fails.
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data.AddSocketDataToFactory(socket_factory());

  // The second endpoint succeeds.
  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.expected_ech_config_list = ech_config_list;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       EchSvcbReliantQuicOnlyAbortTcpAttempt) {
  std::vector<uint8_t> ech_config_list;
  ASSERT_TRUE(MakeTestEchKeys("www.example.org", /*max_name_len=*/128,
                              &ech_config_list));

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  SequencedSocketData tcp_data;
  tcp_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory()->AddSocketDataProvider(&tcp_data);

  AddQuicData();

  StreamRequester requester;
  requester.set_destination(kDefaultDestination).RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Simulate A record resolution. This starts a TCP attempt.
  endpoint_request
      ->set_endpoints({ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint()})
      .CallOnServiceEndpointsUpdated();
  Group& group = pool().GetOrCreateGroupForTesting(requester.GetStreamKey());
  EXPECT_EQ(group.ActiveStreamSocketCount(), 1u);
  ASSERT_FALSE(requester.result().has_value());

  // Simulate HTTPS record resolution. We now know that the endpoint is QUIC
  // only and SVCB-reliant.
  endpoint_request
      ->set_endpoints({ServiceEndpointBuilder()
                           .add_v4("192.0.2.1")
                           .set_alpns({"h3"})
                           .set_ech_config_list(ech_config_list)
                           .endpoint()})
      .set_crypto_ready(true)
      .CallOnServiceEndpointRequestFinished(OK);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  // The TCP attempt should be aborted.
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoQUIC);
  EXPECT_EQ(group.ActiveStreamSocketCount(), 0u);
}

TEST_F(HttpStreamPoolAttemptManagerTest, JobAllowH2Only) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  SequencedSocketData data(reads, writes);
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  HttpStreamKey stream_key = StreamKeyBuilder(kDefaultDestination).Build();
  TestJobDelegate delegate;
  delegate.set_expected_protocol(NextProto::kProtoHTTP2);
  delegate.CreateAndStartJob(pool());
  EXPECT_THAT(delegate.GetResult(), IsOk());
  EXPECT_EQ(delegate.negotiated_protocol(), NextProto::kProtoHTTP2);
}

TEST_F(HttpStreamPoolAttemptManagerTest, JobAllowH2OnlyCancelQuicAttempt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  MockConnectCompleter h3_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&h3_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  MockConnectCompleter h2_completer;
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  SequencedSocketData data(reads, writes);
  data.set_connect_data(MockConnect(&h2_completer));
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  HttpStreamKey stream_key = StreamKeyBuilder(kDefaultDestination).Build();

  // Set the destination is known to support H2. This prevents the
  // AttemptManager from attempting more than one TCP handshake.
  http_server_properties()->SetSupportsSpdy(
      stream_key.destination(), stream_key.network_anonymization_key(),
      /*supports_spdy=*/true);

  // Create the first job that allows all protocols to attempt.
  TestJobDelegate delegate1(stream_key);
  delegate1.set_quic_version(quic_version());
  delegate1.CreateAndStartJob(pool());

  // Create the second job that only allows H2.
  TestJobDelegate delegate2(stream_key);
  delegate2.set_expected_protocol(NextProto::kProtoHTTP2);
  delegate2.CreateAndStartJob(pool());

  base::WeakPtr<AttemptManager> attempt_manager =
      pool()
          .GetGroupForTesting(stream_key)
          ->attempt_manager()
          ->GetWeakPtrForTesting();

  h3_completer.Complete(OK);
  h2_completer.Complete(OK);

  EXPECT_THAT(delegate1.GetResult(), IsOk());
  EXPECT_EQ(delegate1.negotiated_protocol(), NextProto::kProtoHTTP2);
  EXPECT_THAT(delegate2.GetResult(), IsOk());
  EXPECT_EQ(delegate2.negotiated_protocol(), NextProto::kProtoHTTP2);

  EXPECT_THAT(attempt_manager->GetQuicAttemptResultForTesting(),
              Optional(IsError(ERR_ABORTED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest, JobAllowH3OnlyOk) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  AddQuicData();

  TestJobDelegate delegate;
  delegate.set_expected_protocol(NextProto::kProtoQUIC);
  delegate.set_quic_version(quic_version());
  delegate.CreateAndStartJob(pool());
  EXPECT_THAT(delegate.GetResult(), IsOk());
  EXPECT_EQ(delegate.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, JobAllowH3OnlyFail) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  MockQuicData quic_data(quic_version());
  quic_data.AddRead(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  quic_data.AddSocketDataToFactory(socket_factory());

  TestJobDelegate delegate;
  delegate.set_expected_protocol(NextProto::kProtoQUIC);
  delegate.set_quic_version(quic_version());
  delegate.CreateAndStartJob(pool());
  EXPECT_THAT(delegate.GetResult(), IsError(ERR_QUIC_PROTOCOL_ERROR));
}

TEST_F(HttpStreamPoolAttemptManagerTest, JobAllowH3OnlyCancelTcpBasedAttempt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  MockConnectCompleter quic_completer;

  AddQuicData(/*host=*/kDefaultDestination, &quic_completer);

  // Make the TCP attempt stalled forever.
  SequencedSocketData data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&data);

  HttpStreamKey stream_key = StreamKeyBuilder(kDefaultDestination).Build();

  // Create the first job that allows all protocols to attempt.
  TestJobDelegate delegate1(stream_key);
  delegate1.set_quic_version(quic_version());
  delegate1.CreateAndStartJob(pool());

  Group& group = pool().GetOrCreateGroupForTesting(stream_key);
  ASSERT_EQ(group.ActiveStreamSocketCount(), 1u);

  // Create the second job that only allows H3.
  TestJobDelegate delegate2(stream_key);
  delegate2.set_quic_version(quic_version());
  delegate2.set_expected_protocol(NextProto::kProtoQUIC);
  delegate2.CreateAndStartJob(pool());

  ASSERT_EQ(group.ActiveStreamSocketCount(), 0u);

  quic_completer.Complete(OK);

  EXPECT_THAT(delegate1.GetResult(), IsOk());
  EXPECT_EQ(delegate1.negotiated_protocol(), NextProto::kProtoQUIC);

  EXPECT_THAT(delegate2.GetResult(), IsOk());
  EXPECT_EQ(delegate2.negotiated_protocol(), NextProto::kProtoQUIC);
}

// Regression test for crbug.com/384759835.
TEST_F(HttpStreamPoolAttemptManagerTest, DestroyPoolDuringPreconnect) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  MockConnectCompleter completer;
  data.set_connect_data(MockConnect(&completer));
  socket_factory()->AddSocketDataProvider(&data);

  Preconnector preconnector("http://a.test");
  preconnector.Preconnect(pool());
  ASSERT_FALSE(preconnector.result().has_value());

  completer.Complete(OK);
  ASSERT_FALSE(preconnector.result().has_value());

  // Destroy the pool.
  InitializeSession();
  FastForwardUntilNoTasksRemain();
}

// Regression test for crbug.com/384759836.
TEST_F(HttpStreamPoolAttemptManagerTest, DestroyPoolDuringStreamRequest) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.set_destination("http://a.test");
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Cancel request before destroy the pool. This behavior matches the
  // production code.
  requester.ResetRequest();
  // Destroy the pool.
  InitializeSession();
  FastForwardUntilNoTasksRemain();
}

// Regression test for crbug.com/395027296.
// Ensure that canceling attempts after scheduling an attempt completion doesn't
// access destroyed attempts.
TEST_F(HttpStreamPoolAttemptManagerTest, AttemptSyncCompleteCancelAttempt) {
  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  SequencedSocketData data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.set_destination("http://a.test");
  requester.RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Close all connections to cancel in-flight attempts.
  http_network_session()->CloseAllConnections(ERR_ABORTED, "For testing");
  FastForwardUntilNoTasksRemain();
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_ABORTED)));
}

// Regression test for crbug.com/384965448
// Ensure that QUIC attempts are canceled when network change happens.
TEST_F(HttpStreamPoolAttemptManagerTest, NetworkChangeCancelJobs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  // Make the TCP attempt stalled forever.
  SequencedSocketData data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  Group* group = pool().GetGroupForTesting(requester.GetStreamKey());

  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  FastForwardUntilNoTasksRemain();

  quic_completer.Complete(OK);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_NETWORK_CHANGED)));
  // The group should not have active AttemptManager.
  EXPECT_FALSE(group->attempt_manager());
  EXPECT_THAT(requester.associated_attempt_manager()->TcpBasedAttemptCount(),
              0u);
  EXPECT_THAT(
      requester.associated_attempt_manager()->GetQuicAttemptResultForTesting(),
      Optional(IsError(ERR_NETWORK_CHANGED)));

  // Ensure that the group is destroyed after the request is destroyed.
  requester.ResetRequest();
  WaitForAttemptManagerComplete(requester.associated_attempt_manager().get());
  ASSERT_FALSE(pool().GetGroupForTesting(requester.GetStreamKey()));
}

// Regression test for crbug.com/384965448
// Ensure that QUIC attempts are canceled when service endpoint request provided
// results partially and the failed.
TEST_F(HttpStreamPoolAttemptManagerTest,
       ServiceEndpointRequestFailedAfterUpdatedCalled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  // Make the TCP attempt stalled forever.
  SequencedSocketData data;
  data.set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  socket_factory()->AddSocketDataProvider(&data);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  // Notifies partial endpoint results. Triggers QuicAttempt to start.
  endpoint_request->CallOnServiceEndpointsUpdated();

  // Simulates endpoint resolution failure.
  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);

  quic_completer.Complete(OK);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_NAME_NOT_RESOLVED)));
  EXPECT_THAT(requester.associated_attempt_manager()->TcpBasedAttemptCount(),
              0u);
  EXPECT_THAT(
      requester.associated_attempt_manager()->GetQuicAttemptResultForTesting(),
      Optional(IsError(ERR_NAME_NOT_RESOLVED)));
}

// Regression test for crbug.com/384965448
// Ensure that QUIC attempts are canceled when a client certificate is
// required.
TEST_F(HttpStreamPoolAttemptManagerTest, ClientAuthRequiredCancelQuic) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
  ssl.cert_request_info = base::MakeRefCounted<SSLCertRequestInfo>();
  ssl.cert_request_info->host_and_port =
      HostPortPair::FromString(kDefaultDestination);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(),
              Optional(IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED)));
  quic_completer.Complete(OK);
  EXPECT_THAT(requester.associated_attempt_manager()->TcpBasedAttemptCount(),
              0u);
  EXPECT_THAT(
      requester.associated_attempt_manager()->GetQuicAttemptResultForTesting(),
      Optional(IsError(ERR_SSL_CLIENT_AUTH_CERT_NEEDED)));
}

// Regression test for crbug.com/384965448
// Ensure that QUIC attempts are canceled when a certificate error happens.
TEST_F(HttpStreamPoolAttemptManagerTest, CertificateErrorCancelQuic) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  MockConnectCompleter quic_completer;
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(&quic_completer);
  quic_data.AddSocketDataToFactory(socket_factory());

  SequencedSocketData data;
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, ERR_CERT_DATE_INVALID);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_CERT_DATE_INVALID)));
  quic_completer.Complete(OK);
  EXPECT_THAT(requester.associated_attempt_manager()->TcpBasedAttemptCount(),
              0u);
  EXPECT_THAT(
      requester.associated_attempt_manager()->GetQuicAttemptResultForTesting(),
      Optional(IsError(ERR_CERT_DATE_INVALID)));
}

// Regression test for crbug.com/403373872. ServiceEndpointRequest may change
// current endpoints during TCP handshake. This should not cause a crash.
TEST_F(HttpStreamPoolAttemptManagerTest, EndpointDisapperDuringTcpHandshake) {
  MockConnectCompleter completer;
  SequencedSocketData data;
  data.set_connect_data(MockConnect(&completer));
  socket_factory()->AddSocketDataProvider(&data);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();

  // Endpoints become empty tentatively. In production code, this could happen
  // when a DnsTask is canceled during resolution.
  endpoint_request->set_endpoints({});

  // Complete the TCP handshake and the DNS resolution.
  completer.Complete(OK);
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointRequestFinished(OK);

  requester.WaitForResult();
  // TODO(crbug.com/403373872): Ideally the request should succeed.
  EXPECT_THAT(requester.result(), Optional(IsError(ERR_ABORTED)));
}

TEST_F(HttpStreamPoolAttemptManagerTest,
       EndpointCryptoReadyChangeDuringTcpHandshake) {
  MockConnectCompleter completer;
  SequencedSocketData data;
  data.set_connect_data(MockConnect(&completer));
  socket_factory()->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      resolver()->AddFakeRequest();

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_enable_alternative_services(false)
      .RequestStream(pool());

  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();

  // Endpoints become empty and crypto handshake becomes not ready tentatively.
  // In production code, this could happen when a DnsTask is canceled during
  // resolution.
  endpoint_request->set_endpoints({}).set_crypto_ready(false);

  // Complete the TCP handshake. Since crypto handshake is not ready, the
  // request doesn't complete yet.
  completer.Complete(OK);
  ASSERT_FALSE(requester.result().has_value());

  // Complete DNS resolution. The request should succeed.
  endpoint_request
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .set_crypto_ready(true)
      .CallOnServiceEndpointRequestFinished(OK);

  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
}

// Regression test for crbug.com/415488524. A QUIC destination may be marked
// broken after a successful QUIC session attempt. Ensure that a request
// doesn't use QUIC in a such situation.
TEST_F(HttpStreamPoolAttemptManagerTest, QuicBrokenWhenSessionCreated) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  MockConnectCompleter quic_completer;
  AddQuicData(/*host=*/kDefaultDestination, &quic_completer);

  SequencedSocketData tcp_data;
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  ASSERT_FALSE(requester.result().has_value());

  AlternativeService alternative_service(NextProto::kProtoQUIC,
                                         "www.example.org", 443);
  http_server_properties()->MarkAlternativeServiceBroken(
      alternative_service, NetworkAnonymizationKey());

  quic_completer.Complete(OK);
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_NE(requester.negotiated_protocol(), NextProto::kProtoQUIC);
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyOkQuicOk) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  MockConnectCompleter quic_completer;
  AddQuicData(/*host=*/kDefaultDestination, &quic_completer);

  // A TCP based attempt succeeds and uses HTTP/2.
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  SequencedSocketData tcp_data(reads, writes);
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  // A stream request completes with HTTP/2.
  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoHTTP2);

  // Complete `quic_completer` and fast forward to make the QUIC attempt
  // complete.
  quic_completer.Complete(OK);
  FastForwardBy(base::Milliseconds(1));
  EXPECT_THAT(
      requester.associated_attempt_manager()->GetQuicAttemptResultForTesting(),
      Optional(IsOk()));

  // Reset the request so that the AttemptManager can complete after the QUIC
  // attempt is slow.
  requester.ResetRequest();

  // The AttemptManager should complete.
  WaitForAttemptManagerComplete(requester.associated_attempt_manager().get());
  ASSERT_FALSE(requester.associated_attempt_manager());
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdyOkQuicSlowCanceled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // The QUIC attempt stalls forever.
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddSocketDataToFactory(socket_factory());

  // A TCP based attempt succeeds and uses HTTP/2.
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  SequencedSocketData tcp_data(reads, writes);
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  // A stream request completes with HTTP/2.
  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoHTTP2);

  // Reset the request so that the AttemptManager can complete after the QUIC
  // attempt is slow.
  requester.ResetRequest();

  // Simulate connection attempt delay for the QUIC attempt. It cancels the
  // QUIC attempt.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());

  // The AttemptManager should complete.
  ASSERT_FALSE(requester.associated_attempt_manager());
}

TEST_F(HttpStreamPoolAttemptManagerTest, SpdySlowOkQuicCanceled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kAsyncQuicSession);

  resolver()
      ->AddFakeRequest()
      ->add_endpoint(ServiceEndpointBuilder().add_v4("192.0.2.1").endpoint())
      .CompleteStartSynchronously(OK);

  // The QUIC attempt stalls forever.
  MockQuicData quic_data(quic_version());
  quic_data.AddConnect(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data.AddSocketDataToFactory(socket_factory());

  // A TCP based attempt succeeds and uses HTTP/2.
  const MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  const MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 1)};
  SequencedSocketData tcp_data(reads, writes);
  MockConnectCompleter tcp_completer;
  tcp_data.set_connect_data(MockConnect(&tcp_completer));
  socket_factory()->AddSocketDataProvider(&tcp_data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  ssl.next_proto = NextProto::kProtoHTTP2;
  socket_factory()->AddSSLSocketDataProvider(&ssl);

  StreamRequester requester;
  requester.set_destination(kDefaultDestination)
      .set_quic_version(quic_version())
      .RequestStream(pool());

  // Simulate connection attempt delay for both TCP and QUIC attempts. This
  // should not cancel these attempts yet.
  FastForwardBy(HttpStreamPool::GetConnectionAttemptDelay());

  // Complete the TCP attempt. The request should complete with HTTP/2.
  tcp_completer.Complete(OK);
  requester.WaitForResult();
  EXPECT_THAT(requester.result(), Optional(IsOk()));
  EXPECT_EQ(requester.negotiated_protocol(), NextProto::kProtoHTTP2);

  // The QUIC attempt should be canceled.
  EXPECT_FALSE(
      requester.associated_attempt_manager()->quic_attempt_for_testing());

  // Reset the request so that the AttemptManager can complete.
  requester.ResetRequest();

  // The AttemptManager should complete.
  WaitForAttemptManagerComplete(requester.associated_attempt_manager().get());
  ASSERT_FALSE(requester.associated_attempt_manager());
}

}  // namespace net
