// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/connection_migration_information.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/schemeful_site.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_result.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_client_session_peer.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_connectivity_monitor.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_test_packet_maker.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/aes_128_gcm_12_encrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_tag.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/simple_quic_framer.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_COBALT)
#include <algorithm>
#include <cstring>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "net/net_buildflags.h"
#include "net/socket/diff_serv_code_point.h"
#if BUILDFLAG(ENABLE_MULTI_PACKETS_PER_CALL_QUIC_OPTIMIZATIONS)
#include "base/strings/string_number_conversions.h"
#include "net/socket/udp_client_socket.h"
#include "net/socket/udp_server_socket.h"
#endif  // BUILDFLAG(ENABLE_MULTI_PACKETS_PER_CALL_QUIC_OPTIMIZATIONS)
#endif  // BUILDFLAG(IS_COBALT)

using testing::_;

namespace net::test {
#if BUILDFLAG(IS_COBALT)
// Test peer granting controlled inspection of QuicChromiumPacketReader's
// private buffer state and static multi-packet fallback flag.
class QuicChromiumPacketReaderPeer {
 public:
  static bool GetTryReadingMultiplePackets() {
    return QuicChromiumPacketReader::try_reading_multiple_packets_;
  }
  static void SetTryReadingMultiplePackets(bool value) {
    QuicChromiumPacketReader::try_reading_multiple_packets_ = value;
  }
  [[nodiscard]] static base::AutoReset<bool> ScopedSetTryReadingMultiplePackets(
      bool value) {
    return base::AutoReset<bool>(
        &QuicChromiumPacketReader::try_reading_multiple_packets_, value);
  }
  static bool HasReadBuffer(const QuicChromiumPacketReader* reader) {
    return reader->read_buffer_ != nullptr;
  }
  static bool HasReadResultsBuffer(const QuicChromiumPacketReader* reader) {
    return reader->read_results_.buffer != nullptr;
  }
};
#endif  // BUILDFLAG(IS_COBALT)

namespace {

const IPEndPoint kIpEndPoint = IPEndPoint(IPAddress::IPv4AllZeros(), 0);
const char kServerHostname[] = "test.example.com";
const uint16_t kServerPort = 443;
const size_t kMaxReadersPerQuicSession = 5;

const handles::NetworkHandle kDefaultNetworkForTests = 1;
const handles::NetworkHandle kNewNetworkForTests = 2;

class TestingQuicConnection : public quic::QuicConnection {
 public:
  using quic::QuicConnection::QuicConnection;

  void OnKeepAliveTimeout() override {
    keep_alive_timeout_callback_.Run();
    quic::QuicConnection::OnKeepAliveTimeout();
  }

  void SetKeepAliveTimeoutCallback(base::RepeatingCallback<void()> callback) {
    keep_alive_timeout_callback_ = std::move(callback);
  }

 private:
  base::RepeatingCallback<void()> keep_alive_timeout_callback_;
};

// A subclass of QuicChromiumClientSession that allows OnPathDegrading to be
// mocked.
class TestingQuicChromiumClientSession : public QuicChromiumClientSession {
 public:
  using QuicChromiumClientSession::QuicChromiumClientSession;

  MOCK_METHOD(void, OnPathDegrading, (), (override));
  MOCK_METHOD(void, RegisterQuicConnectionClosePayload, (), (override));
  MOCK_METHOD(void, UnregisterQuicConnectionClosePayload, (), (override));

  void ReallyOnPathDegrading() { QuicChromiumClientSession::OnPathDegrading(); }
};

class QuicChromiumClientSessionTest
    : public ::testing::TestWithParam<quic::ParsedQuicVersion>,
      public WithTaskEnvironment {
 public:
  QuicChromiumClientSessionTest()
      : version_(GetParam()),
        config_(quic::test::DefaultQuicConfig()),
        crypto_config_(
            quic::test::crypto_test_utils::ProofVerifierForTesting()),
        default_read_(
            std::make_unique<MockRead>(SYNCHRONOUS, ERR_IO_PENDING, 0)),
        socket_data_(std::make_unique<SequencedSocketData>(
            base::span_from_ref(*default_read_),
            base::span<MockWrite>())),
        helper_(&clock_, &random_),
        transport_security_state_(std::make_unique<TransportSecurityState>()),
        session_key_(kServerHostname,
                     kServerPort,
                     PRIVACY_MODE_DISABLED,
                     ProxyChain::Direct(),
                     SessionUsage::kDestination,
                     SocketTag(),
                     NetworkAnonymizationKey(),
                     SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false),
        destination_(url::kHttpsScheme, kServerHostname, kServerPort),
        default_network_(handles::kInvalidNetworkHandle),
        client_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(&random_),
                      &clock_,
                      kServerHostname,
                      quic::Perspective::IS_CLIENT),
        server_maker_(version_,
                      quic::QuicUtils::CreateRandomConnectionId(&random_),
                      &clock_,
                      kServerHostname,
                      quic::Perspective::IS_SERVER,
                      false) {
    FLAGS_quic_enable_http3_grease_randomness = false;
    quic::QuicEnableVersion(version_);
    // Advance the time, because timers do not like uninitialized times.
    clock_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  void ResetHandleOnError(
      std::unique_ptr<QuicChromiumClientSession::Handle>* handle,
      int net_error) {
    EXPECT_NE(OK, net_error);
    handle->reset();
  }

 protected:
  void Initialize() {
    if (socket_data_) {
      socket_factory_.AddSocketDataProvider(socket_data_.get());
    }
    std::unique_ptr<DatagramClientSocket> socket =
        socket_factory_.CreateDatagramClientSocket(
            DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
    socket->Connect(kIpEndPoint);
    QuicChromiumPacketWriter* writer = new net::QuicChromiumPacketWriter(
        socket.get(), base::SingleThreadTaskRunner::GetCurrentDefault().get());
    auto* connection = new TestingQuicConnection(
        quic::QuicUtils::CreateRandomConnectionId(&random_),
        quic::QuicSocketAddress(), ToQuicSocketAddress(kIpEndPoint), &helper_,
        &alarm_factory_, writer, true, quic::Perspective::IS_CLIENT,
        quic::test::SupportedVersions(version_), connection_id_generator_);
    ping_alarm_ = quic::test::QuicTestAlarmProxy(
        quic::test::QuicConnectionPeer::GetPingAlarm(connection));
    connection->SetKeepAliveTimeoutCallback(
        base::BindRepeating(&QuicChromiumClientSessionTest::OnKeepAliveTimeout,
                            base::Unretained(this)));
    session_ = std::make_unique<TestingQuicChromiumClientSession>(
        connection, std::move(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_, &clock_,
        transport_security_state_.get(), &ssl_config_service_,
        base::WrapUnique(static_cast<QuicServerInfo*>(nullptr)),
        QuicSessionAliasKey(url::SchemeHostPort(), session_key_),
        /*require_confirmation=*/false, migrate_session_early_v2_,
        /*migrate_session_on_network_change_v2=*/false, default_network_,
        quic::QuicTime::Delta::FromMilliseconds(
            kDefaultRetransmittableOnWireTimeout.InMilliseconds()),
        /*migrate_idle_session=*/false, allow_port_migration_,
        kDefaultIdleSessionMigrationPeriod, /*multi_port_probing_interval=*/0,
        kMaxTimeOnNonDefaultNetwork,
        kMaxMigrationsToNonDefaultNetworkOnWriteError,
        kMaxMigrationsToNonDefaultNetworkOnPathDegrading,
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        /*cert_verify_flags=*/0, config_,
        std::make_unique<TestQuicCryptoClientConfigHandle>(&crypto_config_),
        "CONNECTION_UNKNOWN", base::TimeTicks::Now(), base::TimeTicks::Now(),
        base::DefaultTickClock::GetInstance(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*socket_performance_watcher=*/nullptr, ConnectionEndpointMetadata(),
        /*enable_origin_frame=*/true, /*allow_server_preferred_address=*/true,
        MultiplexedSessionCreationInitiator::kUnknown,
        NetLogWithSource::Make(NetLogSourceType::NONE));
    if (connectivity_monitor_) {
      connectivity_monitor_->SetInitialDefaultNetwork(default_network_);
      session_->AddConnectivityObserver(connectivity_monitor_.get());
    }

    scoped_refptr<X509Certificate> cert(
        ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem"));
    verify_details_.cert_verify_result.verified_cert = cert;
    verify_details_.cert_verify_result.is_issued_by_known_root = true;
    session_->Initialize();
    // Blackhole QPACK decoder stream instead of constructing mock writes.
    session_->qpack_decoder()->set_qpack_stream_sender_delegate(
        &noop_qpack_stream_sender_delegate_);
    session_->StartReading();
    writer->set_delegate(session_.get());
  }

  void TearDown() override {
    if (session_) {
      if (connectivity_monitor_) {
        session_->RemoveConnectivityObserver(connectivity_monitor_.get());
      }
      if (session_->connection()->connected()) {
        EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
      }
      session_->CloseSessionOnError(
          ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
          quic::ConnectionCloseBehavior::SILENT_CLOSE);
    }
  }

  void CompleteCryptoHandshake() {
    ASSERT_THAT(session_->CryptoConnect(callback_.callback()), IsOk());
  }

  void OnKeepAliveTimeout() { keep_alive_timeouts_++; }

  std::unique_ptr<QuicChromiumPacketWriter> CreateQuicChromiumPacketWriter(
      DatagramClientSocket* socket,
      QuicChromiumClientSession* session) const {
    auto writer = std::make_unique<QuicChromiumPacketWriter>(
        socket, base::SingleThreadTaskRunner::GetCurrentDefault().get());
    writer->set_delegate(session);
    return writer;
  }

  quic::QuicStreamId GetNthClientInitiatedBidirectionalStreamId(int n) {
    return quic::test::GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  quic::QuicStreamId GetNthServerInitiatedUnidirectionalStreamId(int n) {
    return quic::test::GetNthServerInitiatedUnidirectionalStreamId(
        version_.transport_version, n);
  }

  size_t GetMaxAllowedOutgoingBidirectionalStreams() {
    return quic::test::QuicSessionPeer::ietf_streamid_manager(session_.get())
        ->max_outgoing_bidirectional_streams();
  }

  const quic::ParsedQuicVersion version_;
  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  quic::QuicConfig config_;
  quic::QuicCryptoClientConfig crypto_config_;
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLog::Get(), NetLogSourceType::NONE)};
  MockClientSocketFactory socket_factory_;
  std::unique_ptr<MockRead> default_read_;
  std::unique_ptr<SequencedSocketData> socket_data_;
  quic::MockClock clock_;
  quic::test::MockRandom random_{0};
  QuicChromiumConnectionHelper helper_;
  quic::test::MockAlarmFactory alarm_factory_;
  std::unique_ptr<TransportSecurityState> transport_security_state_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  SSLConfigServiceDefaults ssl_config_service_;
  QuicSessionKey session_key_;
  url::SchemeHostPort destination_;
  std::unique_ptr<TestingQuicChromiumClientSession> session_;
  handles::NetworkHandle default_network_;
  std::unique_ptr<QuicConnectivityMonitor> connectivity_monitor_;
  raw_ptr<quic::QuicConnectionVisitorInterface> visitor_;
  TestCompletionCallback callback_;
  QuicTestPacketMaker client_maker_;
  QuicTestPacketMaker server_maker_;
  ProofVerifyDetailsChromium verify_details_;
  bool migrate_session_early_v2_ = false;
  bool allow_port_migration_ = false;
  quic::test::MockConnectionIdGenerator connection_id_generator_;
  quic::test::NoopQpackStreamSenderDelegate noop_qpack_stream_sender_delegate_;
  int keep_alive_timeouts_ = 0;
  std::optional<quic::test::QuicTestAlarmProxy> ping_alarm_;
};

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicChromiumClientSessionTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

// Basic test of ProofVerifyDetailsChromium is converted to SSLInfo retrieved
// through QuicChromiumClientSession::GetSSLInfo(). Doesn't test some of the
// more complicated fields.
TEST_P(QuicChromiumClientSessionTest, GetSSLInfo1) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  ProofVerifyDetailsChromium details;
  details.is_fatal_cert_error = false;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  details.cert_verify_result.policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  SSLInfo ssl_info;
  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.is_valid());

  EXPECT_EQ(details.is_fatal_cert_error, ssl_info.is_fatal_cert_error);
  EXPECT_TRUE(ssl_info.cert->EqualsIncludingChain(
      details.cert_verify_result.verified_cert.get()));
  EXPECT_EQ(details.cert_verify_result.cert_status, ssl_info.cert_status);
  EXPECT_EQ(details.cert_verify_result.is_issued_by_known_root,
            ssl_info.is_issued_by_known_root);
  EXPECT_EQ(details.cert_verify_result.policy_compliance,
            ssl_info.ct_policy_compliance);
}

// Just like GetSSLInfo1, but uses different values.
TEST_P(QuicChromiumClientSessionTest, GetSSLInfo2) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  ProofVerifyDetailsChromium details;
  details.is_fatal_cert_error = false;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = false;
  details.cert_verify_result.policy_compliance =
      ct::CTPolicyCompliance::CT_POLICY_NOT_ENOUGH_SCTS;

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  SSLInfo ssl_info;
  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.is_valid());

  EXPECT_EQ(details.is_fatal_cert_error, ssl_info.is_fatal_cert_error);
  EXPECT_TRUE(ssl_info.cert->EqualsIncludingChain(
      details.cert_verify_result.verified_cert.get()));
  EXPECT_EQ(details.cert_verify_result.cert_status, ssl_info.cert_status);
  EXPECT_EQ(details.cert_verify_result.is_issued_by_known_root,
            ssl_info.is_issued_by_known_root);
  EXPECT_EQ(details.cert_verify_result.policy_compliance,
            ssl_info.ct_policy_compliance);
}

TEST_P(QuicChromiumClientSessionTest, IsFatalErrorNotSetForNonFatalError) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  SSLInfo ssl_info;
  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.cert_status = CERT_STATUS_DATE_INVALID;
  details.is_fatal_cert_error = false;
  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_FALSE(ssl_info.is_fatal_cert_error);
}

TEST_P(QuicChromiumClientSessionTest, IsFatalErrorSetForFatalError) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  SSLInfo ssl_info;
  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.cert_status = CERT_STATUS_DATE_INVALID;
  details.is_fatal_cert_error = true;
  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  ASSERT_TRUE(session_->GetSSLInfo(&ssl_info));
  EXPECT_TRUE(ssl_info.is_fatal_cert_error);
}

TEST_P(QuicChromiumClientSessionTest, CryptoConnect) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();
}

TEST_P(QuicChromiumClientSessionTest, Handle) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  NetLogWithSource session_net_log = session_->net_log();
  EXPECT_EQ(NetLogSourceType::QUIC_SESSION, session_net_log.source().type);
  EXPECT_EQ(NetLog::Get(), session_net_log.net_log());

  // Set Migration information to the session so that it will be propagated to
  // the handler on init.
  auto migration_info = ConnectionMigrationInformation(
      ConnectionMigrationInformation::NetworkEventCount(
          /*default_network_change=*/1, /*network_disconnected=*/1,
          /*network_connected=*/1, /*path_degrading=*/1));
  session_->SetConnectionMigrationInformationForTesting(migration_info);

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  EXPECT_TRUE(handle->IsConnected());
  EXPECT_FALSE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  IPEndPoint address;
  EXPECT_EQ(OK, handle->GetPeerAddress(&address));
  EXPECT_EQ(kIpEndPoint, address);
  EXPECT_TRUE(handle->CreatePacketBundler().get() != nullptr);

  auto base_migration_info = migration_info;
  EXPECT_EQ(handle->GetConnectionMigrationInfoSinceInit(),
            migration_info - base_migration_info);

  migration_info.event_count.network_connected_num++;
  session_->OnNetworkConnected(kDefaultNetworkForTests);
  EXPECT_EQ(handle->GetConnectionMigrationInfoSinceInit(),
            migration_info - base_migration_info);

  CompleteCryptoHandshake();

  EXPECT_TRUE(handle->OneRttKeysAvailable());

  // Request a stream and verify that a stream was created.
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());

  // Veirfy that the handle works correctly after the session is closed.
  EXPECT_FALSE(handle->IsConnected());
  EXPECT_TRUE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, handle->GetPeerAddress(&address));
  EXPECT_TRUE(handle->CreatePacketBundler().get() == nullptr);
  EXPECT_EQ(handle->GetConnectionMigrationInfoSinceInit(),
            migration_info - base_migration_info);
  {
    // Verify that CreateHandle() works even after the session is closed.
    std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
        session_->CreateHandle(destination_);
    EXPECT_FALSE(handle2->IsConnected());
    EXPECT_TRUE(handle2->OneRttKeysAvailable());
    ASSERT_EQ(ERR_CONNECTION_CLOSED,
              handle2->RequestStream(/*requires_confirmation=*/false,
                                     callback.callback(),
                                     TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  session_.reset();

  // Verify that the handle works correctly after the session is deleted.
  EXPECT_FALSE(handle->IsConnected());
  EXPECT_TRUE(handle->OneRttKeysAvailable());
  EXPECT_EQ(version_, handle->GetQuicVersion());
  EXPECT_EQ(session_key_.server_id(), handle->server_id());
  EXPECT_EQ(session_net_log.source().type, handle->net_log().source().type);
  EXPECT_EQ(session_net_log.source().id, handle->net_log().source().id);
  EXPECT_EQ(session_net_log.net_log(), handle->net_log().net_log());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, handle->GetPeerAddress(&address));
  EXPECT_TRUE(handle->CreatePacketBundler().get() == nullptr);
  ASSERT_EQ(
      ERR_CONNECTION_CLOSED,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(handle->GetConnectionMigrationInfoSinceInit(),
            migration_info - base_migration_info);
}

TEST_P(QuicChromiumClientSessionTest, StreamRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConfirmationRequiredStreamRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/true,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, StreamRequestBeforeConfirmation) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  // Request a stream and verify that a stream was created.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/true, callback.callback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS));

  CompleteCryptoHandshake();

  EXPECT_THAT(callback.WaitForResult(), IsOk());

  EXPECT_TRUE(handle->ReleaseStream() != nullptr);

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, CancelStreamRequestBeforeRelease) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Request a stream and cancel it without releasing the stream.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(OK, handle->RequestStream(/*requires_confirmation=*/false,
                                      callback.callback(),
                                      TRAFFIC_ANNOTATION_FOR_TESTS));
  handle.reset();

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, AsyncStreamRequest) {
  MockQuicData quic_data(version_);
  uint64_t packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  // The open stream limit is set to 50 by
  // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
  // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
  // at the limit of 50.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  // Similarly, requesting the 52nd stream will also send a STREAMS_BLOCKED.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(1),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  // After the STREAMS_BLOCKED is sent, receive a MAX_STREAMS to increase
  // the limit to 100.
  quic_data.AddRead(ASYNC, server_maker_.Packet(1)
                               .AddMaxStreamsFrame(/*control_frame_id=*/1,
                                                   /*stream_count=*/100,
                                                   /*unidirectional=*/false)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that subsequent requests cannot
  // proceed immediately.
  EXPECT_EQ(GetMaxAllowedOutgoingBidirectionalStreams(), 50u);
  for (size_t i = 0; i < 50; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(session_->GetNumActiveStreams(), 50u);

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));
  // Request a second stream and verify that it's also pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            handle2->RequestStream(/*requires_confirmation=*/false,
                                   callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close two stream to open up sending credits.
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
  quic::QuicRstStreamFrame rst2(quic::kInvalidControlFrameId,
                                GetNthClientInitiatedBidirectionalStreamId(1),
                                quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst2);
  // To close the streams completely, we need to also receive STOP_SENDING
  // frames.
  quic::QuicStopSendingFrame stop_sending(
      quic::kInvalidControlFrameId,
      GetNthClientInitiatedBidirectionalStreamId(0),
      quic::QUIC_STREAM_CANCELLED);
  session_->OnStopSendingFrame(stop_sending);
  quic::QuicStopSendingFrame stop_sending2(
      quic::kInvalidControlFrameId,
      GetNthClientInitiatedBidirectionalStreamId(1),
      quic::QUIC_STREAM_CANCELLED);
  session_->OnStopSendingFrame(stop_sending2);

  EXPECT_FALSE(callback.have_result());
  EXPECT_FALSE(callback2.have_result());

  // Pump the message loop to read the packet containing the MAX_STREAMS frame.
  base::RunLoop().RunUntilIdle();

  // Make sure that both requests were unblocked.
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);
  ASSERT_TRUE(callback2.have_result());
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_TRUE(handle2->ReleaseStream() != nullptr);

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Regression test for https://crbug.com/1021938.
// When the connection is closed, there may be tasks queued in the message loop
// to read the last packet, reading that packet should not crash.
TEST_P(QuicChromiumClientSessionTest, ReadAfterConnectionClose) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  // The open stream limit is set to 50 by
  // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
  // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
  // at the limit of 50.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  // This packet will be read after connection is closed.
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddConnectionCloseFrame(
                     quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!")
                 .Build());
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request two streams which will both be pending.
  // In V99 each will generate a max stream id for each attempt.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session_->CreateHandle(destination_);

  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(
          /*requires_confirmation=*/false,
          base::BindOnce(&QuicChromiumClientSessionTest::ResetHandleOnError,
                         base::Unretained(this), &handle2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            handle2->RequestStream(/*requires_confirmation=*/false,
                                   callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  session_->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "Timed out",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // Pump the message loop to read the connection close packet.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle2.get());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ClosedWithAsyncStreamRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  // The open stream limit is set to 50 by
  // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
  // requested, a STREAMS_BLOCKED will be sent, indicating that it's blocked
  // at the limit of 50.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request two streams which will both be pending.
  // In V99 each will generate a max stream id for each attempt.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      session_->CreateHandle(destination_);

  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(
          /*requires_confirmation=*/false,
          base::BindOnce(&QuicChromiumClientSessionTest::ResetHandleOnError,
                         base::Unretained(this), &handle2),
          TRAFFIC_ANNOTATION_FOR_TESTS));

  TestCompletionCallback callback2;
  ASSERT_EQ(ERR_IO_PENDING,
            handle2->RequestStream(/*requires_confirmation=*/false,
                                   callback2.callback(),
                                   TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload);
  session_->connection()->CloseConnection(
      quic::QUIC_NETWORK_IDLE_TIMEOUT, "Timed out",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);

  // Pump the message loop to read the connection close packet.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle2.get());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, CancelPendingStreamRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  // The open stream limit is set to 50 by
  // MockCryptoClientStream::SetConfigNegotiated() so when the 51st stream is
  // requested, a STREAMS_BLOCKED will be sent.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  // This node receives the RST_STREAM+STOP_SENDING, it responds
  // with only a RST_STREAM.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_CANCELLED)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Cancel the pending stream request.
  handle.reset();

  // Close a stream and ensure that no new stream is created.
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
  // We require a STOP_SENDING as well as a RESET_STREAM to fully close the
  // stream.
  quic::QuicStopSendingFrame stop_sending(
      quic::kInvalidControlFrameId,
      GetNthClientInitiatedBidirectionalStreamId(0),
      quic::QUIC_STREAM_CANCELLED);
  session_->OnStopSendingFrame(stop_sending);
  EXPECT_EQ(kMaxOpenStreams - 1, session_->GetNumActiveStreams());

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload);
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseBeforeStreamRequest) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddConnectionCloseFrame(
                     quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!")
                 .Build());

  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Send a ping so that client has outgoing traffic before receiving packets.
  session_->connection()->SendPing();

  // Pump the message loop to read the connection close packet.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload);
  base::RunLoop().RunUntilIdle();

  // Request a stream and verify that it failed.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_CONNECTION_CLOSED,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseBeforeHandshakeConfirmed) {
  if (version_.UsesTls()) {
    // TODO(nharper, b/112643533): Figure out why this test fails when TLS is
    // enabled and fix it.
    return;
  }

  // Force the connection close packet to use long headers with connection ID.
  server_maker_.SetEncryptionLevel(quic::ENCRYPTION_INITIAL);

  MockQuicData quic_data(version_);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddConnectionCloseFrame(
                     quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!")
                 .Build());
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/true, callback.callback(),
                            TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close the connection and verify that the StreamRequest completes with
  // an error.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, ConnectionCloseWithPendingStreamRequest) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1)
                 .AddConnectionCloseFrame(
                     quic::QUIC_CRYPTO_VERSION_NOT_SUPPORTED, "Time to panic!")
                 .Build());
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Send a ping so that client has outgoing traffic before receiving packets.
  session_->connection()->SendPing();

  // Open the maximum number of streams so that a subsequent request
  // can not proceed immediately.
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  }
  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Request a stream and verify that it's pending.
  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close the connection and verify that the StreamRequest completes with
  // an error.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(callback.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, MaxNumStreams) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  // Initial configuration is 50 dynamic streams. Taking into account
  // the static stream (headers), expect to block on when hitting the limit
  // of 50 streams
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_RST_ACKNOWLEDGEMENT)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_RST_ACKNOWLEDGEMENT)
          .Build());
  // For the second CreateOutgoingStream that fails because of hitting the
  // stream count limit.
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(4)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddRead(ASYNC, server_maker_.Packet(1)
                               .AddMaxStreamsFrame(/*control_frame_id=*/1,
                                                   /*stream_count=*/50 + 2,
                                                   /*unidirectional=*/false)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();

  std::vector<QuicChromiumClientStream*> streams;
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientStream* stream =
        QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }
  // This stream, the 51st dynamic stream, can not be opened.
  EXPECT_FALSE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));

  EXPECT_EQ(kMaxOpenStreams, session_->GetNumActiveStreams());

  // Close a stream and ensure I can now open a new one.
  quic::QuicStreamId stream_id = streams[0]->id();
  session_->ResetStream(stream_id, quic::QUIC_RST_ACKNOWLEDGEMENT);

  // Pump data, bringing in the max-stream-id
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));
  quic::QuicRstStreamFrame rst1(quic::kInvalidControlFrameId, stream_id,
                                quic::QUIC_STREAM_NO_ERROR, 0);
  session_->OnRstStream(rst1);
  EXPECT_EQ(kMaxOpenStreams - 1, session_->GetNumActiveStreams());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));
}

// Regression test for crbug.com/968621.
TEST_P(QuicChromiumClientSessionTest, PendingStreamOnRst) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      ASYNC,
      client_maker_.Packet(packet_num++)
          .AddStopSendingFrame(GetNthServerInitiatedUnidirectionalStreamId(0),
                               quic::QUIC_RST_ACKNOWLEDGEMENT)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  quic::QuicStreamFrame data(GetNthServerInitiatedUnidirectionalStreamId(0),
                             false, 1, std::string_view("SP"));
  session_->OnStreamFrame(data);
  EXPECT_EQ(0u, session_->GetNumActiveStreams());
  quic::QuicRstStreamFrame rst(quic::kInvalidControlFrameId,
                               GetNthServerInitiatedUnidirectionalStreamId(0),
                               quic::QUIC_STREAM_CANCELLED, 0);
  session_->OnRstStream(rst);
}

// Regression test for crbug.com/971361.
TEST_P(QuicChromiumClientSessionTest, ClosePendingStream) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      ASYNC,
      client_maker_.Packet(packet_num++)
          .AddStopSendingFrame(GetNthServerInitiatedUnidirectionalStreamId(0),
                               quic::QUIC_RST_ACKNOWLEDGEMENT)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  quic::QuicStreamId id = GetNthServerInitiatedUnidirectionalStreamId(0);
  quic::QuicStreamFrame data(id, false, 1, std::string_view("SP"));
  session_->OnStreamFrame(data);
  EXPECT_EQ(0u, session_->GetNumActiveStreams());
  session_->ResetStream(id, quic::QUIC_STREAM_NO_ERROR);
}

TEST_P(QuicChromiumClientSessionTest, MaxNumStreamsViaRequest) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(2)
          .AddStreamsBlockedFrame(/*control_frame_id=*/1, /*stream_count=*/50,
                                  /*unidirectional=*/false)
          .Build());
  quic_data.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(3)
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_RST_ACKNOWLEDGEMENT)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_RST_ACKNOWLEDGEMENT)
          .Build());
  quic_data.AddRead(ASYNC, server_maker_.Packet(1)
                               .AddMaxStreamsFrame(/*control_frame_id=*/1,
                                                   /*stream_count=*/52,
                                                   /*unidirectional=*/false)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();
  const size_t kMaxOpenStreams = GetMaxAllowedOutgoingBidirectionalStreams();
  std::vector<QuicChromiumClientStream*> streams;
  for (size_t i = 0; i < kMaxOpenStreams; i++) {
    QuicChromiumClientStream* stream =
        QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }

  std::unique_ptr<QuicChromiumClientSession::Handle> handle =
      session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  ASSERT_EQ(
      ERR_IO_PENDING,
      handle->RequestStream(/*requires_confirmation=*/false,
                            callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS));

  // Close a stream and ensure I can now open a new one.
  quic::QuicStreamId stream_id = streams[0]->id();
  session_->ResetStream(stream_id, quic::QUIC_RST_ACKNOWLEDGEMENT);
  quic::QuicRstStreamFrame rst1(quic::kInvalidControlFrameId, stream_id,
                                quic::QUIC_STREAM_NO_ERROR, 0);
  session_->OnRstStream(rst1);
  // Pump data, bringing in the max-stream-id
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(callback.have_result());
  EXPECT_THAT(callback.WaitForResult(), IsOk());
  EXPECT_TRUE(handle->ReleaseStream() != nullptr);
}

TEST_P(QuicChromiumClientSessionTest, GoAwayReceived) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // After receiving a GoAway, I should no longer be able to create outgoing
  // streams.
  session_->OnHttp3GoAway(0);
  EXPECT_EQ(nullptr, QuicChromiumClientSessionPeer::CreateOutgoingStream(
                         session_.get()));
}

TEST_P(QuicChromiumClientSessionTest, CanPool) {
  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  EXPECT_TRUE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_ENABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kDisable,
                     /*require_dns_https_alpn=*/false)));
#if BUILDFLAG(IS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, tag1,
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, tag2,
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
#endif
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED,
                     ProxyChain::FromSchemeHostAndPort(ProxyServer::SCHEME_QUIC,
                                                       "bar", 443),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kProxy, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));

  EXPECT_TRUE(session_->CanPool(
      "mail.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_TRUE(session_->CanPool(
      "mail.example.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "mail.google.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));

  const SchemefulSite kSiteFoo(GURL("http://foo.test/"));

  // Check that NetworkAnonymizationKey is respected when feature is enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    EXPECT_TRUE(session_->CanPool(
        "mail.example.com",
        QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                       SessionUsage::kDestination, SocketTag(),
                       NetworkAnonymizationKey::CreateSameSite(kSiteFoo),
                       SecureDnsPolicy::kAllow,
                       /*require_dns_https_alpn=*/false)));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    EXPECT_FALSE(session_->CanPool(
        "mail.example.com",
        QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                       SessionUsage::kDestination, SocketTag(),
                       NetworkAnonymizationKey::CreateSameSite(kSiteFoo),
                       SecureDnsPolicy::kAllow,
                       /*require_dns_https_alpn=*/false)));
  }
}

// Much as above, but uses a non-empty NetworkAnonymizationKey.
TEST_P(QuicChromiumClientSessionTest, CanPoolWithNetworkAnonymizationKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  const SchemefulSite kSiteFoo(GURL("http://foo.test/"));
  const SchemefulSite kSiteBar(GURL("http://bar.test/"));
  const auto kNetworkAnonymizationKey1 =
      NetworkAnonymizationKey::CreateSameSite(kSiteFoo);
  const auto kNetworkAnonymizationKey2 =
      NetworkAnonymizationKey::CreateSameSite(kSiteBar);

  session_key_ = QuicSessionKey(
      kServerHostname, kServerPort, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
      SessionUsage::kDestination, SocketTag(), kNetworkAnonymizationKey1,
      SecureDnsPolicy::kAllow,
      /*require_dns_https_alpn=*/false);

  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  EXPECT_TRUE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_ENABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
#if BUILDFLAG(IS_ANDROID)
  SocketTag tag1(SocketTag::UNSET_UID, 0x12345678);
  SocketTag tag2(getuid(), 0x87654321);
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, tag1,
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "www.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, tag2,
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
#endif
  EXPECT_TRUE(session_->CanPool(
      "mail.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_TRUE(session_->CanPool(
      "mail.example.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "mail.google.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey1, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));

  EXPECT_FALSE(session_->CanPool(
      "mail.example.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     kNetworkAnonymizationKey2, SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
  EXPECT_FALSE(session_->CanPool(
      "mail.example.com",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
}

TEST_P(QuicChromiumClientSessionTest, ConnectionNotPooledWithDifferentPin) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kStaticKeyPinningEnforcement);
  // Configure the TransportSecurityStateSource so that kPreloadedPKPHost will
  // have static PKP pins set.
  ScopedTransportSecurityStateSource scoped_security_state_source;

  // |net::test_default::kHSTSSource| defines pins for kPreloadedPKPHost.
  // (This hostname must be in the spdy_pooling.pem SAN.)
  const char kPreloadedPKPHost[] = "www.example.org";
  // A hostname without any static state.  (This hostname isn't in
  // spdy_pooling.pem SAN, but that's okay because the
  // ProofVerifyDetailsChromium are faked.)
  const char kNoPinsHost[] = "no-pkp.example.org";

  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  transport_security_state_->EnableStaticPinsForTesting();
  transport_security_state_->SetPinningListAlwaysTimelyForTesting(true);

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  uint8_t bad_pin = 3;
  details.cert_verify_result.public_key_hashes.push_back(
      GetTestHashValue(bad_pin));

  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  QuicChromiumClientSessionPeer::SetHostname(session_.get(), kNoPinsHost);

  EXPECT_FALSE(session_->CanPool(
      kPreloadedPKPHost,
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
}

TEST_P(QuicChromiumClientSessionTest, ConnectionPooledWithMatchingPin) {
  ScopedTransportSecurityStateSource scoped_security_state_source;

  MockQuicData quic_data(version_);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.MakeInitialSettingsPacket(1));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  transport_security_state_->EnableStaticPinsForTesting();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  details.cert_verify_result.is_issued_by_known_root = true;
  HashValue primary_pin(HASH_VALUE_SHA256);
  EXPECT_TRUE(primary_pin.FromString(
      "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY="));
  details.cert_verify_result.public_key_hashes.push_back(primary_pin);

  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);
  QuicChromiumClientSessionPeer::SetHostname(session_.get(), "www.example.org");

  EXPECT_TRUE(session_->CanPool(
      "mail.example.org",
      QuicSessionKey("foo", 1234, PRIVACY_MODE_DISABLED, ProxyChain::Direct(),
                     SessionUsage::kDestination, SocketTag(),
                     NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                     /*require_dns_https_alpn=*/false)));
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocket) {
  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  MockQuicData quic_data(version_);
  int packet_num = 1;
  int peer_packet_num = 1;
  socket_data_.reset();
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddRead(ASYNC, server_maker_.Packet(peer_packet_num++)
                               .AddNewConnectionIdFrame(cid_on_new_path,
                                                        /*sequence_number=*/1u,
                                                        /*retire_prior_to=*/0u)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Make new connection ID available after handshake completion.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  char data[] = "ABCD";
  MockQuicData quic_data2(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.Packet(packet_num++)
                          .AddAckFrame(/*first_received=*/1,
                                       /*largest_received=*/peer_packet_num - 1,
                                       /*smallest_received=*/1)
                          .AddPingFrame()
                          .Build());
  quic_data2.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(packet_num++)
          .AddStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0), false,
                          std::string_view(data))
          .Build());
  quic_data2.AddSocketDataToFactory(&socket_factory_);
  // Create connected socket.
  std::unique_ptr<DatagramClientSocket> new_socket =
      socket_factory_.CreateDatagramClientSocket(DatagramSocket::RANDOM_BIND,
                                                 NetLog::Get(), NetLogSource());
  EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

  // Create reader and writer.
  auto new_reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(new_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_with_source_);
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      CreateQuicChromiumPacketWriter(new_reader->socket(), session_.get()));

  IPEndPoint local_address;
  new_reader->socket()->GetLocalAddress(&local_address);
  IPEndPoint peer_address;
  new_reader->socket()->GetPeerAddress(&peer_address);
  // Migrate session.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  EXPECT_CALL(*session_, RegisterQuicConnectionClosePayload());
  EXPECT_TRUE(session_->MigrateToSocket(
      ToQuicSocketAddress(local_address), ToQuicSocketAddress(peer_address),
      std::move(new_reader), std::move(new_writer)));
  // Spin message loop to complete migration.
  base::RunLoop().RunUntilIdle();

  // Write data to session.
  QuicChromiumClientStream* stream =
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get());
  quic::test::QuicStreamPeer::SendBuffer(stream).SaveStreamData(data);
  quic::test::QuicStreamPeer::SetStreamBytesWritten(4, stream);
  session_->WritevData(stream->id(), 4, 0, quic::NO_FIN,
                       quic::NOT_RETRANSMISSION,
                       quic::ENCRYPTION_FORWARD_SECURE);

  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocketMaxReaders) {
  MockQuicData quic_data(version_);
  socket_data_.reset();
  int packet_num = 1;
  int peer_packet_num = 1;
  quic::QuicConnectionId next_cid = quic::QuicUtils::CreateRandomConnectionId(
      quiche::QuicheRandom::GetInstance());
  uint64_t next_cid_sequence_number = 1u;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC,
                    server_maker_.Packet(peer_packet_num++)
                        .AddNewConnectionIdFrame(
                            next_cid, next_cid_sequence_number,
                            /*retire_prior_to=*/next_cid_sequence_number - 1)
                        .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Make connection ID available for the first migration.
  quic_data.Resume();

  /* Migration succeeds when maximum number of readers is not reached.*/
  for (size_t i = 0; i < kMaxReadersPerQuicSession - 1; ++i) {
    MockQuicData quic_data2(version_);
    client_maker_.set_connection_id(next_cid);
    quic_data2.AddWrite(
        SYNCHRONOUS, client_maker_.Packet(packet_num++)
                         .AddAckFrame(/*first_received=*/1,
                                      /*largest_received=*/peer_packet_num - 1,
                                      /*smallest_received=*/1)
                         .AddPingFrame()
                         .Build());
    quic_data2.AddRead(ASYNC, ERR_IO_PENDING);
    quic_data2.AddWrite(
        ASYNC, client_maker_.Packet(packet_num++)
                   .AddRetireConnectionIdFrame(
                       /*sequence_number=*/next_cid_sequence_number - 1)
                   .Build());
    next_cid = quic::QuicUtils::CreateRandomConnectionId(
        quiche::QuicheRandom::GetInstance());
    ++next_cid_sequence_number;
    quic_data2.AddRead(ASYNC,
                       server_maker_.Packet(peer_packet_num++)
                           .AddNewConnectionIdFrame(
                               next_cid, next_cid_sequence_number,
                               /*retire_prior_to=*/next_cid_sequence_number - 1)
                           .Build());
    quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
    quic_data2.AddSocketDataToFactory(&socket_factory_);

    // Create connected socket.
    std::unique_ptr<DatagramClientSocket> new_socket =
        socket_factory_.CreateDatagramClientSocket(
            DatagramSocket::RANDOM_BIND, NetLog::Get(), NetLogSource());
    EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

    // Create reader and writer.
    auto new_reader = std::make_unique<QuicChromiumPacketReader>(
        std::move(new_socket), &clock_, session_.get(),
        kQuicYieldAfterPacketsRead,
        quic::QuicTime::Delta::FromMilliseconds(
            kQuicYieldAfterDurationMilliseconds),
        net_log_with_source_);
    new_reader->StartReading();
    std::unique_ptr<QuicChromiumPacketWriter> new_writer(
        CreateQuicChromiumPacketWriter(new_reader->socket(), session_.get()));

    IPEndPoint local_address;
    new_reader->socket()->GetLocalAddress(&local_address);
    IPEndPoint peer_address;
    new_reader->socket()->GetPeerAddress(&peer_address);
    // Migrate session.
    EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
    EXPECT_CALL(*session_, RegisterQuicConnectionClosePayload());
    EXPECT_TRUE(session_->MigrateToSocket(
        ToQuicSocketAddress(local_address), ToQuicSocketAddress(peer_address),
        std::move(new_reader), std::move(new_writer)));
    // Spin message loop to complete migration.
    base::RunLoop().RunUntilIdle();
    EXPECT_CALL(*session_, RegisterQuicConnectionClosePayload());
    alarm_factory_.FireAlarm(
        quic::test::QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(
            session_->connection()));
    // Make new connection ID available for subsequent migration.
    quic_data2.Resume();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(quic_data2.AllReadDataConsumed());
    EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
  }

  /* Migration fails when maximum number of readers is reached.*/
  MockQuicData quic_data2(version_);
  quic_data2.AddRead(SYNCHRONOUS, ERR_IO_PENDING);  // Hanging read.
  quic_data2.AddSocketDataToFactory(&socket_factory_);
  // Create connected socket.
  std::unique_ptr<DatagramClientSocket> new_socket =
      socket_factory_.CreateDatagramClientSocket(DatagramSocket::RANDOM_BIND,
                                                 NetLog::Get(), NetLogSource());
  EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

  // Create reader and writer.
  auto new_reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(new_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_with_source_);
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      CreateQuicChromiumPacketWriter(new_reader->socket(), session_.get()));

  IPEndPoint local_address;
  new_reader->socket()->GetLocalAddress(&local_address);
  IPEndPoint peer_address;
  new_reader->socket()->GetPeerAddress(&peer_address);
  EXPECT_FALSE(session_->MigrateToSocket(
      ToQuicSocketAddress(local_address), ToQuicSocketAddress(peer_address),
      std::move(new_reader), std::move(new_writer)));
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, MigrateToSocketReadError) {
  MockQuicData quic_data(version_);
  socket_data_.reset();
  int packet_num = 1;
  int peer_packet_num = 1;

  quic::QuicConnectionId cid_on_new_path =
      quic::test::TestConnectionId(12345678);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddRead(ASYNC, server_maker_.Packet(peer_packet_num++)
                               .AddNewConnectionIdFrame(cid_on_new_path,
                                                        /*sequence_number=*/1u,
                                                        /*retire_prior_to=*/0u)
                               .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_NETWORK_CHANGED);

  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Make new connection ID available after handshake completion.
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  MockQuicData quic_data2(version_);
  client_maker_.set_connection_id(cid_on_new_path);
  quic_data2.AddWrite(SYNCHRONOUS,
                      client_maker_.Packet(packet_num++)
                          .AddAckFrame(/*first_received=*/1,
                                       /*largest_received=*/peer_packet_num - 1,
                                       /*smallest_received=*/1)
                          .AddPingFrame()
                          .Build());
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data2.AddRead(ASYNC, server_maker_.Packet(1).AddPingFrame().Build());
  quic_data2.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data2.AddRead(ASYNC, ERR_NETWORK_CHANGED);
  quic_data2.AddSocketDataToFactory(&socket_factory_);

  // Create connected socket.
  std::unique_ptr<DatagramClientSocket> new_socket =
      socket_factory_.CreateDatagramClientSocket(DatagramSocket::RANDOM_BIND,
                                                 NetLog::Get(), NetLogSource());
  EXPECT_THAT(new_socket->Connect(kIpEndPoint), IsOk());

  // Create reader and writer.
  auto new_reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(new_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_with_source_);
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      CreateQuicChromiumPacketWriter(new_reader->socket(), session_.get()));

  IPEndPoint local_address;
  new_reader->socket()->GetLocalAddress(&local_address);
  IPEndPoint peer_address;
  new_reader->socket()->GetPeerAddress(&peer_address);
  // Store old socket and migrate session.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  EXPECT_CALL(*session_, RegisterQuicConnectionClosePayload());
  EXPECT_TRUE(session_->MigrateToSocket(
      ToQuicSocketAddress(local_address), ToQuicSocketAddress(peer_address),
      std::move(new_reader), std::move(new_writer)));
  // Spin message loop to complete migration.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      quic::test::QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(
          session_->connection())
          ->IsSet());

  // Read error on old socket does not impact session.
  quic_data.Resume();
  EXPECT_TRUE(session_->connection()->connected());
  quic_data2.Resume();

  // Read error on new socket causes session close.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  EXPECT_TRUE(session_->connection()->connected());
  quic_data2.Resume();
  EXPECT_FALSE(session_->connection()->connected());

  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
  EXPECT_TRUE(quic_data2.AllReadDataConsumed());
  EXPECT_TRUE(quic_data2.AllWriteDataConsumed());
}

TEST_P(QuicChromiumClientSessionTest, RetransmittableOnWireTimeout) {
  migrate_session_early_v2_ = true;

  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());

  quic_data.AddRead(
      ASYNC, server_maker_.Packet(1).AddAckFrame(1, packet_num - 1, 1).Build());

  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Open a stream since the connection only sends PINGs to keep a
  // retransmittable packet on the wire if there's an open stream.
  EXPECT_TRUE(
      QuicChromiumClientSessionPeer::CreateOutgoingStream(session_.get()));

  quic::test::QuicTestAlarmProxy alarm(
      quic::test::QuicConnectionPeer::GetPingAlarm(session_->connection()));
  EXPECT_FALSE(alarm.IsSet());

  // Send PING, which will be ACKed by the server. After the ACK, there will be
  // no retransmittable packets on the wire, so the alarm should be set.
  session_->connection()->SendPing();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(alarm.IsSet());
  EXPECT_EQ(
      clock_.ApproximateNow() + quic::QuicTime::Delta::FromMilliseconds(200),
      alarm.deadline());

  // Advance clock and simulate the alarm firing. This should cause a PING to be
  // sent.
  clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(200));
  alarm.Fire();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Regression test for https://crbug.com/1043531.
TEST_P(QuicChromiumClientSessionTest, ResetOnEmptyResponseHeaders) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(
      ASYNC,
      client_maker_.Packet(packet_num++)
          .AddStopSendingFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                               quic::QUIC_STREAM_GENERAL_PROTOCOL_ERROR)
          .AddRstStreamFrame(GetNthClientInitiatedBidirectionalStreamId(0),
                             quic::QUIC_STREAM_GENERAL_PROTOCOL_ERROR)
          .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();

  ProofVerifyDetailsChromium details;
  details.cert_verify_result.verified_cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(details.cert_verify_result.verified_cert.get());

  CompleteCryptoHandshake();
  session_->OnProofVerifyDetailsAvailable(details);

  auto session_handle = session_->CreateHandle(destination_);
  TestCompletionCallback callback;
  EXPECT_EQ(OK, session_handle->RequestStream(/*requires_confirmation=*/false,
                                              callback.callback(),
                                              TRAFFIC_ANNOTATION_FOR_TESTS));

  auto stream_handle = session_handle->ReleaseStream();
  EXPECT_TRUE(stream_handle->IsOpen());

  auto* stream = quic::test::QuicSessionPeer::GetOrCreateStream(
      session_.get(), stream_handle->id());

  const quic::QuicHeaderList empty_response_headers;
  static_cast<quic::QuicSpdyStream*>(stream)->OnStreamHeaderList(
      /* fin = */ false, /* frame_len = */ 0, empty_response_headers);

  // QuicSpdyStream::OnStreamHeaderList() calls
  // QuicChromiumClientStream::OnInitialHeadersComplete() with the empty
  // header list, and QuicChromiumClientStream signals an error.
  quiche::HttpHeaderBlock header_block;
  int rv = stream_handle->ReadInitialHeaders(&header_block,
                                             CompletionOnceCallback());
  EXPECT_THAT(rv, IsError(net::ERR_QUIC_PROTOCOL_ERROR));

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  base::RunLoop().RunUntilIdle();
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// This test verifies that when handles::NetworkHandle is not supported and
// there is no network change, session reports to the connectivity monitor
// correctly on path degrading detection and recovery.
TEST_P(QuicChromiumClientSessionTest,
       DegradingWithoutNetworkChange_NoNetworkHandle) {
  // Add a connectivity monitor for testing.
  default_network_ = handles::kInvalidNetworkHandle;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  Initialize();

  // Fire path degrading detection.
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // Fire again.
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Close the session but keep the session around, the connectivity monitor
  // will not remove the tracking immediately.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Delete the session will remove the degrading count in connectivity
  // monitor.
  session_.reset();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
}

// This test verifies that when multi-port and port migration is enabled, path
// degrading won't trigger port migration.
TEST_P(QuicChromiumClientSessionTest, DegradingWithMultiPortEnabled) {
  // Default network is always set to handles::kInvalidNetworkHandle.
  default_network_ = handles::kInvalidNetworkHandle;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);
  allow_port_migration_ = true;
  auto options = config_.SendConnectionOptions();
  config_.SetClientConnectionOptions(quic::QuicTagVector{quic::kMPQC});
  config_.SetConnectionOptionsToSend(options);

  Initialize();
  EXPECT_TRUE(session_->connection()->multi_port_stats());

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  EXPECT_EQ(
      UNKNOWN_CAUSE,
      QuicChromiumClientSessionPeer::GetCurrentMigrationCause(session_.get()));
}

// This test verifies that when the handles::NetworkHandle is not supported, and
// there are speculated network change reported via OnIPAddressChange, session
// still reports to the connectivity monitor correctly on path degrading
// detection and recovery.
TEST_P(QuicChromiumClientSessionTest, DegradingWithIPAddressChange) {
  // Default network is always set to handles::kInvalidNetworkHandle.
  default_network_ = handles::kInvalidNetworkHandle;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  Initialize();

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // When handles::NetworkHandle is not supported, network change is notified
  // via IP address change.
  connectivity_monitor_->OnIPAddressChanged();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // When handles::NetworkHandle is not supported and IP address changes,
  // session either goes away or gets closed. When it goes away,
  // reporting to connectivity monitor is disabled.
  connectivity_monitor_->OnSessionGoingAwayOnIPAddressChange(session_.get());

  // Even if session detects recovery or degradation, this session is no longer
  // on the default network and connectivity monitor will not update.
  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_.reset();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
}

// This test verifies that when handles::NetworkHandle is supported but
// migration is not supported and there's no network change, session reports to
// connectivity monitor correctly on path degrading detection or recovery.
// Default network change is currently reported with valid
// handles::NetworkHandles while session's current network interface is tracked
// by |default_network_|.
TEST_P(QuicChromiumClientSessionTest,
       DegradingOnDeafultNetwork_WithoutMigration) {
  default_network_ = kDefaultNetworkForTests;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  Initialize();

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Close the session but keep the session around, the connectivity monitor
  // should not remove the count immediately.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Delete the session will remove the degrading count in connectivity
  // monitor.
  session_.reset();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
}

// This test verifies that when handles::NetworkHandle is supported but
// migrations is not supported and there is network changes, session reports to
// the connectivity monitor correctly on path degrading detection or recovery.
TEST_P(QuicChromiumClientSessionTest,
       DegradingWithDeafultNetworkChange_WithoutMigration) {
  default_network_ = kDefaultNetworkForTests;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  Initialize();

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  // Simulate the default network change.
  connectivity_monitor_->OnDefaultNetworkUpdated(kNewNetworkForTests);
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
  session_->OnNetworkMadeDefault(kNewNetworkForTests);

  // Session stays on the old default network, and recovers.
  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // Session degrades again on the old default.
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // Simulate that default network switches back to the old default.
  connectivity_monitor_->OnDefaultNetworkUpdated(kDefaultNetworkForTests);
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
  session_->OnNetworkMadeDefault(kDefaultNetworkForTests);

  // Session recovers again on the (old) default.
  session_->OnForwardProgressMadeAfterPathDegrading();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());

  // Session degrades again on the (old) default.
  session_->ReallyOnPathDegrading();
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  session_->CloseSessionOnError(ERR_ABORTED, quic::QUIC_INTERNAL_ERROR,
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_EQ(1u, connectivity_monitor_->GetNumDegradingSessions());

  session_.reset();
  EXPECT_EQ(0u, connectivity_monitor_->GetNumDegradingSessions());
}

TEST_P(QuicChromiumClientSessionTest, WriteErrorDuringCryptoConnect) {
  // Add a connectivity monitor for testing.
  default_network_ = kDefaultNetworkForTests;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  // Use unmocked crypto stream to do crypto connect.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData quic_data(version_);
  // Trigger a packet write error when sending packets in crypto connect.
  quic_data.AddWrite(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  ASSERT_THAT(session_->CryptoConnect(callback_.callback()),
              IsError(ERR_QUIC_HANDSHAKE_FAILED));
  // Verify error count is properly recorded.
  EXPECT_EQ(1u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_ADDRESS_UNREACHABLE));
  EXPECT_EQ(0u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_CONNECTION_RESET));

  // Simulate a default network change, write error stats should be reset.
  connectivity_monitor_->OnDefaultNetworkUpdated(kNewNetworkForTests);
  EXPECT_EQ(0u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_ADDRESS_UNREACHABLE));
}

TEST_P(QuicChromiumClientSessionTest, WriteErrorAfterHandshakeConfirmed) {
  // Add a connectivity monitor for testing.
  default_network_ = handles::kInvalidNetworkHandle;
  connectivity_monitor_ =
      std::make_unique<QuicConnectivityMonitor>(default_network_);

  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  // When sending the PING packet, trigger a packet write error.
  quic_data.AddWrite(SYNCHRONOUS, ERR_CONNECTION_RESET);
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  // Send a ping so that client has outgoing traffic before receiving packets.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  session_->connection()->SendPing();

  // Verify error count is properly recorded.
  EXPECT_EQ(1u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_CONNECTION_RESET));
  EXPECT_EQ(0u, connectivity_monitor_->GetCountForWriteErrorCode(
                    ERR_ADDRESS_UNREACHABLE));

  connectivity_monitor_->OnIPAddressChanged();

  // If network handle is supported, IP Address change is a no-op. Otherwise it
  // clears all stats.
  size_t expected_error_count =
      NetworkChangeNotifier::AreNetworkHandlesSupported() ? 1u : 0u;
  EXPECT_EQ(
      expected_error_count,
      connectivity_monitor_->GetCountForWriteErrorCode(ERR_CONNECTION_RESET));
}

// Much like above, but checking that ECN marks are reported.
TEST_P(QuicChromiumClientSessionTest, ReportsReceivedEcn) {
  MockQuicData mock_quic_data(version_);
  int write_packet_num = 1, read_packet_num = 0;
  quic::QuicEcnCounts ecn(1, 0, 0);  // 1 ECT(0) packet received
  mock_quic_data.AddWrite(
      ASYNC, client_maker_.MakeInitialSettingsPacket(write_packet_num++));
  mock_quic_data.AddRead(
      ASYNC, server_maker_.MakeInitialSettingsPacket(read_packet_num++));
  server_maker_.set_ecn_codepoint(quic::ECN_ECT0);
  mock_quic_data.AddRead(
      ASYNC, server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                           .AddAckFrame(0, 1, 0, ecn)
                                           .Build());
  server_maker_.set_ecn_codepoint(quic::ECN_ECT1);
  mock_quic_data.AddRead(
      ASYNC, server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  server_maker_.set_ecn_codepoint(quic::ECN_CE);
  mock_quic_data.AddRead(
      ASYNC, server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  ecn.ect1 = 1;
  ecn.ce = 1;
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                           .AddAckFrame(0, 3, 0, ecn)
                                           .Build());
  mock_quic_data.AddRead(SYNCHRONOUS, ERR_IO_PENDING);

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_quic_data.AllReadDataConsumed());
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
}

#if BUILDFLAG(IS_COBALT)
class ScopedTryReadingMultiplePackets {
 public:
  explicit ScopedTryReadingMultiplePackets(bool value)
      : previous_(
            QuicChromiumPacketReaderPeer::GetTryReadingMultiplePackets()) {
    QuicChromiumPacketReaderPeer::SetTryReadingMultiplePackets(value);
  }
  ~ScopedTryReadingMultiplePackets() {
    QuicChromiumPacketReaderPeer::SetTryReadingMultiplePackets(previous_);
  }

 private:
  const bool previous_;
};

class BatchEcnMockUDPClientSocket : public MockUDPClientSocket {
 public:
  struct QueuedPacket {
    std::string payload;
    uint8_t tos;
  };

  BatchEcnMockUDPClientSocket(const IPEndPoint& local_address,
                              const IPEndPoint& peer_address,
                              std::vector<QueuedPacket> packets);
  ~BatchEcnMockUDPClientSocket() override;

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadMultiplePackets(ReadPacketResults* results,
                          int read_buffer_size,
                          CompletionOnceCallback callback) override;
  int GetLocalAddress(IPEndPoint* address) const override;
  int GetPeerAddress(IPEndPoint* address) const override;

  int read_multiple_packets_calls() const {
    return read_multiple_packets_calls_;
  }
  int single_read_calls() const { return single_read_calls_; }

 private:
  IPEndPoint local_address_;
  IPEndPoint peer_address_;
  std::vector<QueuedPacket> queued_packets_;
  int read_multiple_packets_calls_ = 0;
  int single_read_calls_ = 0;
};

BatchEcnMockUDPClientSocket::BatchEcnMockUDPClientSocket(
    const IPEndPoint& local_address,
    const IPEndPoint& peer_address,
    std::vector<QueuedPacket> packets)
    : MockUDPClientSocket(nullptr, nullptr),
      local_address_(local_address),
      peer_address_(peer_address),
      queued_packets_(std::move(packets)) {}

BatchEcnMockUDPClientSocket::~BatchEcnMockUDPClientSocket() = default;

int BatchEcnMockUDPClientSocket::Read(IOBuffer* buf,
                                      int buf_len,
                                      CompletionOnceCallback callback) {
  ++single_read_calls_;
  return ERR_IO_PENDING;
}

int BatchEcnMockUDPClientSocket::ReadMultiplePackets(
    ReadPacketResults* results,
    int read_buffer_size,
    CompletionOnceCallback callback) {
  ++read_multiple_packets_calls_;
  if (queued_packets_.empty()) {
    return ERR_IO_PENDING;
  }

  const size_t count = queued_packets_.size();
  CHECK(results->AcquireBuffer(static_cast<int>(count), read_buffer_size));

  auto packet_slots = UNSAFE_BUFFERS(base::span(results->packets, count));
  for (size_t i = 0; i < count; ++i) {
    const std::string& src = queued_packets_[i].payload;
    CHECK_LE(src.size(), static_cast<size_t>(read_buffer_size));
    UNSAFE_BUFFERS(base::span(packet_slots[i].buffer, src.size()))
        .copy_from(base::span<const char>(src));
    packet_slots[i].result = static_cast<int>(src.size());
    packet_slots[i].tos = queued_packets_[i].tos;
  }
  results->result = static_cast<int>(count);
  queued_packets_.clear();
  return results->result;
}

int BatchEcnMockUDPClientSocket::GetLocalAddress(IPEndPoint* address) const {
  *address = local_address_;
  return OK;
}

int BatchEcnMockUDPClientSocket::GetPeerAddress(IPEndPoint* address) const {
  *address = peer_address_;
  return OK;
}

// User journey: When Cobalt receives a multi-packet QUIC burst via recvmmsg
// carrying mixed DSCP and ECN marks (ECT0, ECT1, CE), each packet's 2-bit ECN
// codepoint must be decoded and reported accurately in outgoing QUIC ACK frames
// for both RFCv1 and RFCv2 connections.
TEST_P(QuicChromiumClientSessionTest, ReportsReceivedEcnMultiplePackets) {
  Socket::ReadPacketResults::ClearBufferPoolForTesting();
  MockQuicData mock_quic_data(version_);
  int write_packet_num = 1, read_packet_num = 0;
  quic::QuicEcnCounts first_ack_ecn(1, 0, 0);
  quic::QuicEcnCounts second_ack_ecn(1, 1, 1);

  mock_quic_data.AddWrite(
      ASYNC, client_maker_.MakeInitialSettingsPacket(write_packet_num++));
  mock_quic_data.AddRead(
      ASYNC, server_maker_.MakeInitialSettingsPacket(read_packet_num++));
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                           .AddAckFrame(0, 1, 0, first_ack_ecn)
                                           .Build());
  mock_quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                           .AddAckFrame(0, 3, 0, second_ack_ecn)
                                           .Build());
  mock_quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  mock_quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  auto packet_ect0 =
      server_maker_.Packet(read_packet_num++).AddPingFrame().Build();
  auto packet_ect1 =
      server_maker_.Packet(read_packet_num++).AddPingFrame().Build();
  auto packet_ce =
      server_maker_.Packet(read_packet_num++).AddPingFrame().Build();

  mock_quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();
  base::RunLoop().RunUntilIdle();

  IPEndPoint local_address;
  IPEndPoint peer_address;
  ASSERT_THAT(session_->GetDefaultSocket()->GetLocalAddress(&local_address),
              IsOk());
  ASSERT_THAT(session_->GetDefaultSocket()->GetPeerAddress(&peer_address),
              IsOk());

  // Combine non-zero DSCP values with each ECN codepoint to verify DSCP bits
  // are masked out rather than cast directly into QuicEcnCodepoint.
  std::vector<BatchEcnMockUDPClientSocket::QueuedPacket> batch;
  batch.push_back({std::string(packet_ect0->data(), packet_ect0->length()),
                   static_cast<uint8_t>((DSCP_CS2 << 2) | ECN_ECT0)});
  batch.push_back({std::string(packet_ect1->data(), packet_ect1->length()),
                   static_cast<uint8_t>((DSCP_AF41 << 2) | ECN_ECT1)});
  batch.push_back({std::string(packet_ce->data(), packet_ce->length()),
                   static_cast<uint8_t>((DSCP_CS5 << 2) | ECN_CE)});

  auto batch_socket = std::make_unique<BatchEcnMockUDPClientSocket>(
      local_address, peer_address, std::move(batch));
  BatchEcnMockUDPClientSocket* raw_batch_socket = batch_socket.get();

  ScopedTryReadingMultiplePackets enable_multi_packet_reads(true);
  QuicChromiumPacketReader batch_reader(
      std::move(batch_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_with_source_);
  batch_reader.StartReading();
  base::RunLoop().RunUntilIdle();

  EXPECT_GE(raw_batch_socket->read_multiple_packets_calls(), 1);
  EXPECT_EQ(raw_batch_socket->single_read_calls(), 0);
  const quic::QuicEcnCounts& received_ecn =
      session_->connection()->GetStats().num_ecn_marks_received;
  EXPECT_EQ(received_ecn.ect0, 1u);
  EXPECT_EQ(received_ecn.ect1, 1u);
  EXPECT_EQ(received_ecn.ce, 1u);
  EXPECT_TRUE(mock_quic_data.AllWriteDataConsumed());
  Socket::ReadPacketResults::ClearBufferPoolForTesting();
}
#endif  // BUILDFLAG(IS_COBALT)

TEST_P(QuicChromiumClientSessionTest, OnOriginFrame) {
  const std::string kExampleOrigin1 = "https://www.example.com";
  const std::string kExampleOrigin2 = "https://www.example.com:443";
  const std::string kExampleOrigin3 = "https://www.example.com:8443";
  const std::string kExampleOrigin4 = "http://www.example.com:8080";
  const std::string kInvalidOrigin1 = "https://www.example.com/";
  const std::string kInvalidOrigin2 = "www.example.com";

  GURL url1(base::StrCat({kExampleOrigin1, "/"}));
  url::SchemeHostPort origin1(url1);
  ASSERT_TRUE(origin1.IsValid());
  GURL url2(base::StrCat({kExampleOrigin2, "/"}));
  url::SchemeHostPort origin2(url2);
  ASSERT_TRUE(origin2.IsValid());
  GURL url3(base::StrCat({kExampleOrigin3, "/"}));
  url::SchemeHostPort origin3(url3);
  ASSERT_TRUE(origin3.IsValid());
  GURL url4(base::StrCat({kExampleOrigin4, "/"}));
  url::SchemeHostPort origin4(url4);
  ASSERT_TRUE(origin4.IsValid());

  quic::OriginFrame frame;

  Initialize();

  ASSERT_TRUE(session_->received_origins().empty());

  frame.origins.push_back(kExampleOrigin1);
  session_->OnOriginFrame(frame);
  EXPECT_EQ(1u, session_->received_origins().size());
  EXPECT_TRUE(session_->received_origins().count(origin1));
  EXPECT_TRUE(session_->received_origins().count(origin2));
  EXPECT_FALSE(session_->received_origins().count(origin3));
  EXPECT_FALSE(session_->received_origins().count(origin4));

  frame.origins.push_back(kExampleOrigin2);
  frame.origins.push_back(kInvalidOrigin1);
  frame.origins.push_back(kInvalidOrigin2);
  frame.origins.push_back(kExampleOrigin3);
  frame.origins.push_back(kExampleOrigin4);
  session_->OnOriginFrame(frame);
  EXPECT_EQ(3u, session_->received_origins().size());

  EXPECT_TRUE(session_->received_origins().count(origin1));
  EXPECT_TRUE(session_->received_origins().count(origin2));
  EXPECT_TRUE(session_->received_origins().count(origin3));
  EXPECT_TRUE(session_->received_origins().count(origin4));
}

TEST_P(QuicChromiumClientSessionTest, SettingEcn) {
  quic::QuicTagVector copt;
  copt.push_back(quic::kPRGC);  // Prague Cubic congestion control, uses ECT(1).
  config_.SetClientConnectionOptions(copt);
  MockQuicData quic_data(version_);
  char packet[] = {
      0x40, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x01,
      0x00, 0x00, 0x01, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
      0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
  };
  quic_data.AddReadPause();
  quic_data.AddWrite(ASYNC, std::make_unique<quic::QuicEncryptedPacket>(
                                packet, sizeof(packet)));
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  auto* mock_socket = reinterpret_cast<const MockUDPClientSocket*>(
      session_->GetDefaultSocket());

  // The first packet write changes the socket ECN setting.
  EXPECT_EQ(mock_socket->outgoing_ecn(), ECN_NOT_ECT);
  session_->connection()->SetEncrypter(
      quic::ENCRYPTION_FORWARD_SECURE,
      std::make_unique<quic::test::TaggingEncrypter>(0x0a));
  session_->connection()->SendPing();
  EXPECT_EQ(mock_socket->outgoing_ecn(), ECN_ECT1);
}

TEST_P(QuicChromiumClientSessionTest,
       RegisterQuicConnectionClosePayloadOnTlsHandshakeConfirmed) {
  Initialize();

  // Call OnTlsHandshakeConfirmed and verify RegisterQuicConnectionClosePayload
  // is called.
  // Since it's not easy to trigger OnTlsHandshakeConfirmed by injecting read
  // data, this test directly calls OnTlsHandshakeConfirmed().
  EXPECT_CALL(*session_, RegisterQuicConnectionClosePayload());
  session_->OnTlsHandshakeConfirmed();
}

TEST_P(QuicChromiumClientSessionTest,
       RegisterQuicConnectionClosePayloadOnServerConnectionIdRetired) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddWrite(ASYNC,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  // Set retire_prior_to=1 to retire the currently active connection ID.
  quic::QuicConnectionId new_cid = quic::test::TestConnectionId(123456);
  quic_data.AddRead(ASYNC, server_maker_.Packet(1)
                               .AddNewConnectionIdFrame(new_cid,
                                                        /*sequence_number=*/1u,
                                                        /*retire_prior_to=*/1u)
                               .Build());

  client_maker_.set_connection_id(new_cid);
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(packet_num++)
                                      .AddAckFrame(/*first_received=*/1,
                                                   /*largest_received=*/1,
                                                   /*smallest_received=*/1)
                                      .AddRetireConnectionIdFrame(
                                          /*sequence_number=*/0)
                                      .Build());

  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);
  Initialize();
  CompleteCryptoHandshake();

  // Make new connection ID available
  quic_data.Resume();
  base::RunLoop().RunUntilIdle();

  // Retire server connection ID and verify that
  // RegisterQuicConnectionClosePayload is called.
  EXPECT_CALL(*session_, RegisterQuicConnectionClosePayload());
  alarm_factory_.FireAlarm(
      quic::test::QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(
          session_->connection()));

  // Close the connection and verify that UnregisterQuicConnectionClosePayload
  // is called.
  EXPECT_CALL(*session_, UnregisterQuicConnectionClosePayload());
  quic_data.Resume();
  EXPECT_TRUE(quic_data.AllReadDataConsumed());
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Test whether we arm pings after send a ping for periodic keep alive.
TEST_P(QuicChromiumClientSessionTest, SendPeriodicPings) {
  MockQuicData quic_data(version_);
  int packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(packet_num++));
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.Packet(packet_num++).AddPingFrame().Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  // Set the keep alive ping to a relatively short timeout.
  session_->connection()->set_keep_alive_ping_timeout(
      quic::QuicTime::Delta::FromSeconds(10));
  // Set connection keep alive true.
  session_->SetPeriodicConnectionKeepAlive(true);

  EXPECT_TRUE(ping_alarm_.has_value());
  EXPECT_FALSE(ping_alarm_->IsSet());
  CompleteCryptoHandshake();

  // Check that we do not have any outstanding streams, but have the alarm set.
  EXPECT_EQ(0u, session_->GetNumActiveStreams());
  EXPECT_TRUE(ping_alarm_->IsSet());

  EXPECT_EQ(quic::QuicTime::Delta::FromSeconds(10),
            ping_alarm_->deadline() - clock_.ApproximateNow());
  clock_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(10));
  ping_alarm_->Fire();

  // Check if we call keep alive timeout even without any outstanding packets.
  CHECK_EQ(1, keep_alive_timeouts_);
}

#if BUILDFLAG(IS_COBALT)
class BatchingDatagramClientSocket : public DatagramClientSocket {
 public:
  struct PacketBatch {
    IoMode mode;
    std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> packets;
  };

  explicit BatchingDatagramClientSocket(
      std::unique_ptr<DatagramClientSocket> inner)
      : inner_(std::move(inner)) {}

  void AddPacketBatch(
      IoMode mode,
      std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> packets) {
    batches_.push_back({mode, std::move(packets)});
  }

  void CompletePendingBatch() {
    ASSERT_FALSE(pending_callback_.is_null());
    ASSERT_LT(next_batch_, batches_.size());
    int rv = PopulateResults(batches_[next_batch_++].packets, pending_results_,
                             pending_read_buffer_size_);
    pending_results_ = nullptr;
    std::move(pending_callback_).Run(rv);
  }

  int ReadMultiplePackets(ReadPacketResults* results,
                          int read_buffer_size,
                          CompletionOnceCallback callback) override {
    if (next_batch_ >= batches_.size() ||
        batches_[next_batch_].mode == ASYNC) {
      pending_results_ = results;
      pending_read_buffer_size_ = read_buffer_size;
      pending_callback_ = std::move(callback);
      return ERR_IO_PENDING;
    }
    return PopulateResults(batches_[next_batch_++].packets, results,
                           read_buffer_size);
  }

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return inner_->Read(buf, buf_len, std::move(callback));
  }
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return inner_->Write(buf, buf_len, std::move(callback), traffic_annotation);
  }
  int SetReceiveBufferSize(int32_t size) override {
    return inner_->SetReceiveBufferSize(size);
  }
  int SetSendBufferSize(int32_t size) override {
    return inner_->SetSendBufferSize(size);
  }
  void Close() override {
    pending_callback_.Reset();
    pending_results_ = nullptr;
    inner_->Close();
  }
  int GetPeerAddress(IPEndPoint* address) const override {
    return inner_->GetPeerAddress(address);
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    return inner_->GetLocalAddress(address);
  }
  void UseNonBlockingIO() override { inner_->UseNonBlockingIO(); }
  int SetDoNotFragment() override { return inner_->SetDoNotFragment(); }
  int SetRecvTos() override { return inner_->SetRecvTos(); }
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override {
    return inner_->SetTos(dscp, ecn);
  }
  void SetMsgConfirm(bool confirm) override { inner_->SetMsgConfirm(confirm); }
  const NetLogWithSource& NetLog() const override { return inner_->NetLog(); }
  DscpAndEcn GetLastTos() const override { return inner_->GetLastTos(); }
  int Connect(const IPEndPoint& address) override {
    return inner_->Connect(address);
  }
  int ConnectUsingNetwork(handles::NetworkHandle network,
                          const IPEndPoint& address) override {
    return inner_->ConnectUsingNetwork(network, address);
  }
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override {
    return inner_->ConnectUsingDefaultNetwork(address);
  }
  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override {
    return inner_->ConnectAsync(address, std::move(callback));
  }
  int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) override {
    return inner_->ConnectUsingNetworkAsync(network, address,
                                            std::move(callback));
  }
  int ConnectUsingDefaultNetworkAsync(
      const IPEndPoint& address,
      CompletionOnceCallback callback) override {
    return inner_->ConnectUsingDefaultNetworkAsync(address,
                                                   std::move(callback));
  }
  handles::NetworkHandle GetBoundNetwork() const override {
    return inner_->GetBoundNetwork();
  }
  void ApplySocketTag(const SocketTag& tag) override {
    inner_->ApplySocketTag(tag);
  }
  int SetMulticastInterface(uint32_t interface_index) override {
    return inner_->SetMulticastInterface(interface_index);
  }

 private:
  int PopulateResults(
      const std::vector<std::unique_ptr<quic::QuicEncryptedPacket>>& packets,
      ReadPacketResults* results,
      int read_buffer_size) {
    CHECK(results->AcquireBuffer(static_cast<int>(packets.size()),
                                 read_buffer_size));
    size_t stride = static_cast<size_t>(read_buffer_size);
    results->result = static_cast<int>(packets.size());
    for (size_t i = 0; i < packets.size(); ++i) {
      base::span<const char> src = base::span(packets[i]->AsStringPiece());
      CHECK_LE(src.size(), stride);
      UNSAFE_TODO(
          std::memcpy(results->packets[i].buffer, src.data(), src.size()));
      UNSAFE_TODO(results->packets[i].result) = static_cast<int>(src.size());
      UNSAFE_TODO(results->packets[i].tos) = 0;
    }
    return results->result;
  }

  std::unique_ptr<DatagramClientSocket> inner_;
  std::vector<PacketBatch> batches_;
  size_t next_batch_ = 0;
  raw_ptr<ReadPacketResults> pending_results_ = nullptr;
  int pending_read_buffer_size_ = 0;
  CompletionOnceCallback pending_callback_;
};

// Verifies that when DAOP is not configured, Cobalt's multi-packet recvmmsg
// reader preserves baseline behavior and emits mid-batch standalone ACKs.
TEST_P(QuicChromiumClientSessionTest,
       MultiPacketReadsSendImmediateAcksWithoutDaop) {
  MockQuicData quic_data(version_);
  int write_packet_num = 1;
  int read_packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(write_packet_num++));
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                      .AddAckFrame(1, 2, 1)
                                      .Build());
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                      .AddAckFrame(1, 4, 1)
                                      .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  MockQuicData batch_backing_data(version_);
  batch_backing_data.AddRead(ASYNC, ERR_IO_PENDING);
  batch_backing_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  batch_backing_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  base::AutoReset<bool> reset_multi_packet_reads =
      QuicChromiumPacketReaderPeer::ScopedSetTryReadingMultiplePackets(true);

  std::unique_ptr<DatagramClientSocket> inner_socket =
      socket_factory_.CreateDatagramClientSocket(
          DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  ASSERT_THAT(inner_socket->Connect(kIpEndPoint), IsOk());
  auto batch_socket =
      std::make_unique<BatchingDatagramClientSocket>(std::move(inner_socket));
  std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> batch;
  for (int i = 0; i < 4; ++i) {
    batch.push_back(
        server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  }
  batch_socket->AddPacketBatch(SYNCHRONOUS, std::move(batch));

  QuicChromiumPacketReader batch_reader(
      std::move(batch_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_with_source_);
  batch_reader.StartReading();

  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Verifies that when DAOP is configured on a Cobalt QUIC client session, both
// synchronous and asynchronous ReadMultiplePackets (recvmmsg) batches hold
// standalone ACK-only packets until the entire batch has been processed.
TEST_P(QuicChromiumClientSessionTest,
       ScopedAckDeferrerBatchesMultiPacketReadsWithDaop) {
  config_.SetClientConnectionOptions({quic::kDAOP});

  MockQuicData quic_data(version_);
  int write_packet_num = 1;
  int read_packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(write_packet_num++));
  // Synchronous 4-packet batch produces a single consolidated ACK for 1..4.
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                      .AddAckFrame(1, 4, 1)
                                      .Build());
  // Asynchronous 4-packet batch produces a single consolidated ACK for 1..8.
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                      .AddAckFrame(1, 8, 1)
                                      .Build());
  quic_data.AddRead(ASYNC, ERR_IO_PENDING);
  quic_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  quic_data.AddSocketDataToFactory(&socket_factory_);

  MockQuicData batch_backing_data(version_);
  batch_backing_data.AddRead(ASYNC, ERR_IO_PENDING);
  batch_backing_data.AddRead(ASYNC, ERR_CONNECTION_CLOSED);
  batch_backing_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  base::AutoReset<bool> reset_multi_packet_reads =
      QuicChromiumPacketReaderPeer::ScopedSetTryReadingMultiplePackets(true);

  std::unique_ptr<DatagramClientSocket> inner_socket =
      socket_factory_.CreateDatagramClientSocket(
          DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  ASSERT_THAT(inner_socket->Connect(kIpEndPoint), IsOk());
  auto batch_socket =
      std::make_unique<BatchingDatagramClientSocket>(std::move(inner_socket));
  BatchingDatagramClientSocket* raw_batch_socket = batch_socket.get();

  std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> sync_batch;
  for (int i = 0; i < 4; ++i) {
    sync_batch.push_back(
        server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  }
  batch_socket->AddPacketBatch(SYNCHRONOUS, std::move(sync_batch));

  std::vector<std::unique_ptr<quic::QuicEncryptedPacket>> async_batch;
  for (int i = 0; i < 4; ++i) {
    async_batch.push_back(
        server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  }
  batch_socket->AddPacketBatch(ASYNC, std::move(async_batch));

  QuicChromiumPacketReader batch_reader(
      std::move(batch_socket), &clock_, session_.get(),
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_with_source_);
  batch_reader.StartReading();
  raw_batch_socket->CompletePendingBatch();

  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}

// Verifies that on platforms falling back to single-packet socket->Read(),
// DAOP still coalesces standalone ACK-only packets across synchronous socket
// drain bursts in QuicChromiumPacketReader::OnReadComplete and StartReading.
TEST_P(QuicChromiumClientSessionTest,
       ScopedAckDeferrerBatchesSynchronousSinglePacketReadDrainWithDaop) {
  config_.SetClientConnectionOptions({quic::kDAOP});

  MockQuicData quic_data(version_);
  int write_packet_num = 1;
  int read_packet_num = 1;
  quic_data.AddWrite(SYNCHRONOUS,
                     client_maker_.MakeInitialSettingsPacket(write_packet_num++));
  quic_data.AddReadPause();
  quic_data.AddRead(
      ASYNC, server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  quic_data.AddRead(
      SYNCHRONOUS,
      server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  quic_data.AddRead(
      SYNCHRONOUS,
      server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  quic_data.AddRead(
      SYNCHRONOUS,
      server_maker_.Packet(read_packet_num++).AddPingFrame().Build());
  quic_data.AddReadPauseForever();
  quic_data.AddWrite(SYNCHRONOUS, client_maker_.Packet(write_packet_num++)
                                      .AddAckFrame(1, 4, 1)
                                      .Build());
  quic_data.AddSocketDataToFactory(&socket_factory_);

  Initialize();
  CompleteCryptoHandshake();

  quic_data.Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(quic_data.AllWriteDataConsumed());
}
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace

#if BUILDFLAG(IS_COBALT)
namespace {

// Minimal fake DatagramClientSocket driving deterministic multi-packet and
// single-packet reads without real OS sockets.
class FakeMultiPacketDatagramSocket : public MockUDPClientSocket {
 public:
  FakeMultiPacketDatagramSocket() : MockUDPClientSocket() {}

  int ReadMultiplePackets(ReadPacketResults* results,
                          int read_buffer_size,
                          CompletionOnceCallback callback) override {
    ++read_multiple_calls_;
    incoming_buffers_.push_back(results->buffer.get());
    entry_pool_sizes_.push_back(
        Socket::ReadPacketResults::GetBufferPoolSizeForTesting());
    if (return_not_implemented_) {
      return ERR_NOT_IMPLEMENTED;
    }
    if (!queued_errors_.empty()) {
      const int err = queued_errors_.front();
      queued_errors_.erase(queued_errors_.begin());
      return err;
    }
    if (queued_batches_.empty()) {
      return ERR_IO_PENDING;
    }
    std::vector<std::string> batch = std::move(queued_batches_.front());
    queued_batches_.erase(queued_batches_.begin());
    if (!results->AcquireBuffer(static_cast<int>(batch.size()),
                                read_buffer_size)) {
      return ERR_INVALID_ARGUMENT;
    }
    outgoing_buffers_.push_back(results->buffer.get());
    results->result = static_cast<int>(batch.size());
    for (size_t i = 0; i < batch.size(); ++i) {
      UNSAFE_TODO(std::memcpy(results->packets[i].buffer, batch[i].data(),
                              batch[i].size()));
      UNSAFE_TODO(results->packets[i].result) =
          static_cast<int>(batch[i].size());
    }
    return results->result;
  }

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    ++single_read_calls_;
    if (queued_single_reads_.empty()) {
      return ERR_IO_PENDING;
    }
    std::string payload = std::move(queued_single_reads_.front());
    queued_single_reads_.erase(queued_single_reads_.begin());
    const int n = std::min(buf_len, static_cast<int>(payload.size()));
    UNSAFE_TODO(
        std::memcpy(buf->data(), payload.data(), static_cast<size_t>(n)));
    return n;
  }

  void Close() override { closed_ = true; }

  int GetLocalAddress(IPEndPoint* address) const override {
    *address = IPEndPoint(IPAddress::IPv4Localhost(), 12345);
    return OK;
  }

  int GetPeerAddress(IPEndPoint* address) const override {
    *address = IPEndPoint(IPAddress::IPv4Localhost(), 443);
    return OK;
  }

  void QueueBatch(std::vector<std::string> packets) {
    queued_batches_.push_back(std::move(packets));
  }
  void QueueReadError(int error) { queued_errors_.push_back(error); }
  void QueueSingleRead(std::string payload) {
    queued_single_reads_.push_back(std::move(payload));
  }
  void set_return_not_implemented(bool value) {
    return_not_implemented_ = value;
  }
  int read_multiple_calls() const { return read_multiple_calls_; }
  int single_read_calls() const { return single_read_calls_; }
  bool closed() const { return closed_; }
  const std::vector<const IOBufferWithSize*>& incoming_buffers() const {
    return incoming_buffers_;
  }
  const std::vector<const IOBufferWithSize*>& outgoing_buffers() const {
    return outgoing_buffers_;
  }
  const std::vector<size_t>& entry_pool_sizes() const {
    return entry_pool_sizes_;
  }

 private:
  std::vector<std::vector<std::string>> queued_batches_;
  std::vector<int> queued_errors_;
  std::vector<std::string> queued_single_reads_;
  std::vector<const IOBufferWithSize*> incoming_buffers_;
  std::vector<const IOBufferWithSize*> outgoing_buffers_;
  std::vector<size_t> entry_pool_sizes_;
  bool return_not_implemented_ = false;
  int read_multiple_calls_ = 0;
  int single_read_calls_ = 0;
  bool closed_ = false;
};

// Visitor that records delivered packets and read errors while allowing tests
// to inject reentrant callbacks during OnPacket().
class RecordingPacketReaderVisitor : public QuicChromiumPacketReader::Visitor {
 public:
  bool OnReadError(int result, const DatagramClientSocket* socket) override {
    read_errors_.push_back(result);
    if (on_read_error_callback_) {
      return on_read_error_callback_.Run(result);
    }
    return false;
  }

  bool OnPacket(const quic::QuicReceivedPacket& packet,
                const quic::QuicSocketAddress& local_address,
                const quic::QuicSocketAddress& peer_address) override {
    packets_.emplace_back(packet.data(), packet.length());
    if (on_packet_with_data_callback_) {
      return on_packet_with_data_callback_.Run(packet);
    }
    if (on_packet_callback_) {
      return on_packet_callback_.Run();
    }
    return true;
  }

  void set_on_read_error_callback(base::RepeatingCallback<bool(int)> cb) {
    on_read_error_callback_ = std::move(cb);
  }
  void set_on_packet_callback(base::RepeatingCallback<bool()> cb) {
    on_packet_callback_ = std::move(cb);
  }
  void set_on_packet_with_data_callback(
      base::RepeatingCallback<bool(const quic::QuicReceivedPacket&)> cb) {
    on_packet_with_data_callback_ = std::move(cb);
  }
  const std::vector<std::string>& packets() const { return packets_; }
  const std::vector<int>& read_errors() const { return read_errors_; }

 private:
  std::vector<std::string> packets_;
  std::vector<int> read_errors_;
  base::RepeatingCallback<bool(int)> on_read_error_callback_;
  base::RepeatingCallback<bool()> on_packet_callback_;
  base::RepeatingCallback<bool(const quic::QuicReceivedPacket&)>
      on_packet_with_data_callback_;
};

// Test fixture for Cobalt-specific QuicChromiumPacketReader buffer lifecycle
// and CloseSocket() behavior.
class QuicChromiumPacketReaderCobaltTest : public ::testing::Test,
                                           public WithTaskEnvironment {
 protected:
  void SetUp() override {
    Socket::ReadPacketResults::ClearBufferPoolForTesting();
  }
  void TearDown() override {
    Socket::ReadPacketResults::ClearBufferPoolForTesting();
  }

  quic::MockClock clock_;
  NetLogWithSource net_log_;
};

// Verifies the QUIC packet-reader receive journey on Cobalt: constructing a
// reader allocates neither the single-packet fallback buffer nor a multi-packet
// buffer, and every delivered batch recycles its scratch buffer back to the
// pool immediately—even if OnPacket() destroys the reader reentrantly.
TEST_F(QuicChromiumPacketReaderCobaltTest, LazyReadBufferAndBufferRecycling) {
  ScopedTryReadingMultiplePackets enable_multi_packet(true);

  auto socket = std::make_unique<FakeMultiPacketDatagramSocket>();
  socket->QueueBatch({"alpha", "beta"});

  RecordingPacketReaderVisitor visitor;
  auto reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket), &clock_, &visitor, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  // Neither buffer is allocated at construction.
  EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));

  bool member_cleared_during_on_packet = true;
  visitor.set_on_packet_callback(base::BindRepeating(
      [](QuicChromiumPacketReader* r, bool* cleared) {
        *cleared =
            *cleared && !QuicChromiumPacketReaderPeer::HasReadResultsBuffer(r);
        return true;
      },
      reader.get(), &member_cleared_during_on_packet));

  reader->StartReading();

  EXPECT_TRUE(member_cleared_during_on_packet);
  EXPECT_THAT(visitor.packets(), testing::ElementsAre("alpha", "beta"));
  EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);

  // Verify reentrant reader destruction inside OnPacket() safely stops
  // iteration and still returns the backing buffer to the pool.
  Socket::ReadPacketResults::ClearBufferPoolForTesting();
  auto socket2 = std::make_unique<FakeMultiPacketDatagramSocket>();
  socket2->QueueBatch({"first-destroy", "never-delivered"});
  RecordingPacketReaderVisitor visitor2;
  auto reader2 = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket2), &clock_, &visitor2, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);
  visitor2.set_on_packet_callback(base::BindRepeating(
      [](std::unique_ptr<QuicChromiumPacketReader>* r) {
        r->reset();
        return true;
      },
      &reader2));

  reader2->StartReading();

  EXPECT_EQ(reader2, nullptr);
  EXPECT_THAT(visitor2.packets(), testing::ElementsAre("first-destroy"));
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);
}

// Verifies closing a QUIC connection right after a burst read caused the
// packet reader to yield to the message loop: CloseSocket() cancels the queued
// callback and releases buffered packets immediately rather than delivering
// post-close packets or issuing another ReadMultiplePackets() call.
TEST_F(QuicChromiumPacketReaderCobaltTest,
       CloseSocketCancelsYieldedTaskAndReleasesBuffers) {
  ScopedTryReadingMultiplePackets enable_multi_packet(true);

  auto socket = std::make_unique<FakeMultiPacketDatagramSocket>();
  FakeMultiPacketDatagramSocket* raw_socket = socket.get();
  socket->QueueBatch({"batch-1"});
  socket->QueueBatch({"batch-2-yielded"});

  RecordingPacketReaderVisitor visitor;
  // yield_after_packets=1 processes batch 1 inline and posts a task for
  // batch 2.
  auto reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket), &clock_, &visitor, /*yield_after_packets=*/1,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  reader->StartReading();
  EXPECT_THAT(visitor.packets(), testing::ElementsAre("batch-1"));
  EXPECT_TRUE(QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));
  const int calls_before_close = raw_socket->read_multiple_calls();

  reader->CloseSocket();
  EXPECT_TRUE(raw_socket->closed());
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));
  EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(visitor.packets(), testing::ElementsAre("batch-1"));
  EXPECT_TRUE(visitor.read_errors().empty());
  EXPECT_EQ(raw_socket->read_multiple_calls(), calls_before_close);
}

// Verifies the single-packet fallback journey when a platform or socket
// reports ERR_NOT_IMPLEMENTED for ReadMultiplePackets: the reader lazily
// allocates read_buffer_ only when falling back to Read(), and drops it on
// CloseSocket().
TEST_F(QuicChromiumPacketReaderCobaltTest,
       FallbackToSinglePacketLazilyAllocatesReadBuffer) {
  ScopedTryReadingMultiplePackets enable_multi_packet(true);

  auto socket = std::make_unique<FakeMultiPacketDatagramSocket>();
  FakeMultiPacketDatagramSocket* raw_socket = socket.get();
  socket->set_return_not_implemented(true);

  RecordingPacketReaderVisitor visitor;
  auto reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket), &clock_, &visitor, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
  reader->StartReading();

  EXPECT_EQ(raw_socket->read_multiple_calls(), 1);
  EXPECT_EQ(raw_socket->single_read_calls(), 1);
  EXPECT_FALSE(QuicChromiumPacketReaderPeer::GetTryReadingMultiplePackets());
  EXPECT_TRUE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));

  reader->CloseSocket();
  EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
}

// Verifies receiving a single-packet fallback datagram whose processing closes
// the socket (for example a QUIC CONNECTION_CLOSE on platforms without
// recvmmsg): the packet payload stays readable throughout OnPacket() even
// after CloseSocket() drops the reader's buffer reference.
TEST_F(QuicChromiumPacketReaderCobaltTest,
       FallbackSinglePacketKeepsBufferAliveDuringOnPacketCloseSocket) {
  ScopedTryReadingMultiplePackets disable_multi_packet(false);

  auto socket = std::make_unique<FakeMultiPacketDatagramSocket>();
  socket->QueueSingleRead("close-frame-payload");

  RecordingPacketReaderVisitor visitor;
  auto reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket), &clock_, &visitor, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  std::string observed_after_close;
  visitor.set_on_packet_with_data_callback(base::BindRepeating(
      [](QuicChromiumPacketReader* r, std::string* after_close,
         const quic::QuicReceivedPacket& packet) {
        r->CloseSocket();
        EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(r));
        after_close->assign(packet.data(), packet.length());
        return false;
      },
      reader.get(), &observed_after_close));

  reader->StartReading();

  EXPECT_EQ(observed_after_close, "close-frame-payload");
  EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
}

// Verifies the multi-batch QUIC burst-drain journey (for example when several
// UDP batches are already queued during 4K video segment delivery): the packet
// reader keeps the same scratch buffer across back-to-back loop iterations
// without pool round-trips and returns it to the pool only when draining hits
// ERR_IO_PENDING or the visitor stops reading.
TEST_F(QuicChromiumPacketReaderCobaltTest,
       BurstDrainReusesBufferWithoutPoolRoundTrip) {
  ScopedTryReadingMultiplePackets enable_multi_packet(true);

  auto socket = std::make_unique<FakeMultiPacketDatagramSocket>();
  FakeMultiPacketDatagramSocket* raw_socket = socket.get();
  socket->QueueBatch({"b1-p1", "b1-p2"});
  socket->QueueBatch({"b2-p1", "b2-p2"});

  RecordingPacketReaderVisitor visitor;
  auto reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket), &clock_, &visitor, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  reader->StartReading();

  EXPECT_THAT(visitor.packets(),
              testing::ElementsAre("b1-p1", "b1-p2", "b2-p1", "b2-p2"));
  ASSERT_EQ(raw_socket->outgoing_buffers().size(), 2u);
  const IOBufferWithSize* burst_buf = raw_socket->outgoing_buffers()[0];
  ASSERT_NE(burst_buf, nullptr);
  EXPECT_EQ(raw_socket->outgoing_buffers()[1], burst_buf);
  // Batch 1 starts empty; batch 2 and the trailing ERR_IO_PENDING check reuse
  // the retained buffer without releasing it back to the pool mid-burst.
  EXPECT_THAT(raw_socket->incoming_buffers(),
              testing::ElementsAre(nullptr, burst_buf, burst_buf));
  EXPECT_THAT(raw_socket->entry_pool_sizes(), testing::ElementsAre(0u, 0u, 0u));
  // Once ERR_IO_PENDING completes the burst, the buffer returns to the pool.
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);

  // If OnPacket() returns false to stop reading, the scratch buffer is
  // recycled to the pool immediately rather than retained on the reader.
  Socket::ReadPacketResults::ClearBufferPoolForTesting();
  auto socket2 = std::make_unique<FakeMultiPacketDatagramSocket>();
  socket2->QueueBatch({"stop-here", "ignored"});
  RecordingPacketReaderVisitor visitor2;
  visitor2.set_on_packet_callback(base::BindRepeating([]() { return false; }));
  auto reader2 = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket2), &clock_, &visitor2, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  reader2->StartReading();

  EXPECT_THAT(visitor2.packets(), testing::ElementsAre("stop-here"));
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader2.get()));
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);
}

// Verifies that a transport read error (such as ICMP port-unreachable
// ERR_CONNECTION_REFUSED) during multi-packet reading reports to OnReadError()
// and returns cleanly without falling through to single-packet Read() or
// lazily allocating read_buffer_ on a closed socket.
TEST_F(QuicChromiumPacketReaderCobaltTest,
       NonFallbackErrorDoesNotFallThroughToSinglePacketRead) {
  ScopedTryReadingMultiplePackets enable_multi_packet(true);

  auto socket = std::make_unique<FakeMultiPacketDatagramSocket>();
  FakeMultiPacketDatagramSocket* raw_socket = socket.get();
  socket->QueueReadError(ERR_CONNECTION_REFUSED);

  RecordingPacketReaderVisitor visitor;
  auto reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket), &clock_, &visitor, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);
  visitor.set_on_read_error_callback(base::BindRepeating(
      [](QuicChromiumPacketReader* r, int /*result*/) {
        r->CloseSocket();
        return false;
      },
      reader.get()));

  reader->StartReading();

  EXPECT_THAT(visitor.read_errors(),
              testing::ElementsAre(ERR_CONNECTION_REFUSED));
  EXPECT_TRUE(raw_socket->closed());
  EXPECT_EQ(raw_socket->single_read_calls(), 0);
  EXPECT_TRUE(QuicChromiumPacketReaderPeer::GetTryReadingMultiplePackets());
  EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));
}

// Verifies receiving a multi-packet QUIC batch whose first packet closes the
// socket inside OnPacket() (for example a CONNECTION_CLOSE frame): the active
// packet payload stays readable after CloseSocket(), subsequent packets in the
// batch are not delivered, and the scratch buffer recycles back to the pool.
TEST_F(QuicChromiumPacketReaderCobaltTest,
       MultiPacketOnPacketCloseSocketKeepsPayloadValidAndRecyclesBuffer) {
  ScopedTryReadingMultiplePackets enable_multi_packet(true);

  auto socket = std::make_unique<FakeMultiPacketDatagramSocket>();
  FakeMultiPacketDatagramSocket* raw_socket = socket.get();
  socket->QueueBatch({"close-frame-pkt", "never-delivered"});

  RecordingPacketReaderVisitor visitor;
  auto reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket), &clock_, &visitor, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  std::string observed_after_close;
  visitor.set_on_packet_with_data_callback(base::BindRepeating(
      [](QuicChromiumPacketReader* r, std::string* after_close,
         const quic::QuicReceivedPacket& packet) {
        r->CloseSocket();
        EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadResultsBuffer(r));
        after_close->assign(packet.data(), packet.length());
        // Even if the visitor returns true, CloseSocket() invalidated weak
        // pointers so ProcessMultiplePacketReadResult() must stop iterating.
        return true;
      },
      reader.get(), &observed_after_close));

  reader->StartReading();

  EXPECT_TRUE(raw_socket->closed());
  EXPECT_EQ(observed_after_close, "close-frame-pkt");
  EXPECT_THAT(visitor.packets(), testing::ElementsAre("close-frame-pkt"));
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);
}

#if BUILDFLAG(ENABLE_MULTI_PACKETS_PER_CALL_QUIC_OPTIMIZATIONS)
// Spy DatagramClientSocket forwarding to a real UDPClientSocket so tests can
// observe the actual recvmmsg batch size and backing buffer sizes driven by
// QuicChromiumPacketReader::StartReading().
class SpyRealUDPClientSocket : public MockUDPClientSocket {
 public:
  explicit SpyRealUDPClientSocket(std::unique_ptr<UDPClientSocket> inner)
      : MockUDPClientSocket(), inner_(std::move(inner)) {}

  int ReadMultiplePackets(ReadPacketResults* results,
                          int read_buffer_size,
                          CompletionOnceCallback callback) override {
    int rv = inner_->ReadMultiplePackets(results, read_buffer_size,
                                         std::move(callback));
    observed_results_.push_back(rv);
    observed_buffer_sizes_.push_back(
        results->buffer ? results->buffer->span().size() : 0u);
    return rv;
  }

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return inner_->Read(buf, buf_len, std::move(callback));
  }

  void Close() override { inner_->Close(); }

  int GetLocalAddress(IPEndPoint* address) const override {
    return inner_->GetLocalAddress(address);
  }

  int GetPeerAddress(IPEndPoint* address) const override {
    return inner_->GetPeerAddress(address);
  }

  const std::vector<int>& observed_results() const { return observed_results_; }
  const std::vector<size_t>& observed_buffer_sizes() const {
    return observed_buffer_sizes_;
  }

 private:
  std::unique_ptr<UDPClientSocket> inner_;
  std::vector<int> observed_results_;
  std::vector<size_t> observed_buffer_sizes_;
};

// Verifies the end-to-end QUIC packet-reader journey over a real localhost UDP
// socket with CobaltQuicReadBatch enabled: StartReading() drives recvmmsg()
// with the configured batch cap, delivers all queued datagrams in order, and
// releases its scratch buffer back to the pool once idle on ERR_IO_PENDING.
TEST_F(QuicChromiumPacketReaderCobaltTest,
       RealUdpSocketSpyVerifiesConfiguredRecvmmsgBatchSize) {
  ScopedTryReadingMultiplePackets enable_multi_packet(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltQuicReadBatch, {{"CobaltQuicReadBatch_packets", "4"}});

  constexpr int kExpectedMaxBatch = 4;
  constexpr int kNumPackets = 10;

  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  auto server =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  ASSERT_THAT(server->Listen(server_address), IsOk());
  ASSERT_THAT(server->GetLocalAddress(&server_address), IsOk());

  auto real_client = std::make_unique<UDPClientSocket>(
      DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  ASSERT_THAT(real_client->Connect(server_address), IsOk());
  ASSERT_THAT(real_client->SetReceiveBufferSize(64 * 1024), IsOk());

  IPEndPoint client_address;
  ASSERT_THAT(real_client->GetLocalAddress(&client_address), IsOk());

  for (int i = 0; i < kNumPackets; ++i) {
    std::string payload = "udp-pkt-" + base::NumberToString(i);
    auto buf = base::MakeRefCounted<StringIOBuffer>(payload);
    TestCompletionCallback send_cb;
    int rv = server->SendTo(buf.get(), static_cast<int>(payload.size()),
                            client_address, send_cb.callback());
    ASSERT_EQ(send_cb.GetResult(rv), static_cast<int>(payload.size()));
  }

  auto spy = std::make_unique<SpyRealUDPClientSocket>(std::move(real_client));
  SpyRealUDPClientSocket* raw_spy = spy.get();

  RecordingPacketReaderVisitor visitor;
  auto reader = std::make_unique<QuicChromiumPacketReader>(
      std::move(spy), &clock_, &visitor, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  reader->StartReading();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(static_cast<int>(visitor.packets().size()), kNumPackets);
  for (int i = 0; i < kNumPackets; ++i) {
    EXPECT_EQ(visitor.packets()[i], "udp-pkt-" + base::NumberToString(i));
  }

  const size_t expected_buf_bytes =
      static_cast<size_t>(kExpectedMaxBatch) *
      (sizeof(Socket::ReadPacketResult) + quic::kMaxIncomingPacketSize + 1);
  int positive_calls = 0;
  for (size_t i = 0; i < raw_spy->observed_results().size(); ++i) {
    const int rv = raw_spy->observed_results()[i];
    if (rv > 0) {
      ++positive_calls;
      EXPECT_LE(rv, kExpectedMaxBatch);
      EXPECT_EQ(raw_spy->observed_buffer_sizes()[i], expected_buf_bytes);
    } else {
      EXPECT_EQ(rv, ERR_IO_PENDING);
      EXPECT_EQ(raw_spy->observed_buffer_sizes()[i], 0u);
    }
  }
  EXPECT_GE(positive_calls, 3);
  EXPECT_FALSE(
      QuicChromiumPacketReaderPeer::HasReadResultsBuffer(reader.get()));
  EXPECT_FALSE(QuicChromiumPacketReaderPeer::HasReadBuffer(reader.get()));
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);
}
#endif  // BUILDFLAG(ENABLE_MULTI_PACKETS_PER_CALL_QUIC_OPTIMIZATIONS)

}  // namespace
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace net::test
