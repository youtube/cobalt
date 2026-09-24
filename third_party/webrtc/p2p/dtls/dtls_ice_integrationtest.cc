/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <cstdint>
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "api/candidate.h"
#include "api/crypto/crypto_options.h"
#include "api/dtls_transport_interface.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/ice_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/scoped_refptr.h"
#include "api/test/create_network_emulation_manager.h"
#include "api/test/network_emulation_manager.h"
#include "api/test/rtc_error_matchers.h"
#include "api/test/simulated_network.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/connection_info.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_transport_channel.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "p2p/client/basic_port_allocator.h"
#include "p2p/dtls/dtls_transport.h"
#include "p2p/test/fake_ice_transport.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/random.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

ABSL_FLAG(int32_t,
          long_running_seed,
          7788,
          "0 means use time(0) as seed (i.e non deterministic)");
ABSL_FLAG(int32_t, long_running_run_time_minutes, 7, "");
ABSL_FLAG(bool, long_running_send_data, false, "");

ABSL_FLAG(int32_t, bench_iterations, 0, "");
ABSL_FLAG(std::vector<std::string>,
          bench_pct_loss,
          std::vector<std::string>({"0", "5", "10", "25"}),
          "Packet loss in percent");
ABSL_FLAG(std::vector<std::string>,
          bench_server_candidates,
          std::vector<std::string>({"1", "2"}),
          "Server candidates");

namespace webrtc {
namespace {
constexpr int kDefaultTimeout = 30000;

using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Not;

std::set<int> ToIntSet(const std::vector<std::string>& args) {
  std::set<int> out;
  for (const auto& arg : args) {
    out.insert(std::stoi(arg));
  }
  return out;
}

struct EndpointConfig {
  SSLProtocolVersion max_protocol_version;
  IceRole ice_role;
  SSLRole ssl_role;
  bool dtls_in_stun = false;
  bool pqc = false;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const EndpointConfig& config) {
    sink.Append("[ dtls: ");
    sink.Append(config.ssl_role == SSL_SERVER ? "server/" : "client/");
    switch (config.max_protocol_version) {
      case SSL_PROTOCOL_DTLS_10:
        sink.Append("1.0");
        break;
      case SSL_PROTOCOL_DTLS_12:
        sink.Append("1.2");
        break;
      case SSL_PROTOCOL_DTLS_13:
        sink.Append("1.3");
        break;
      default:
        sink.Append("<unknown>");
        break;
    }
    absl::Format(&sink, " ice: ");
    sink.Append(config.ice_role == ICEROLE_CONTROLLED ? "controlled"
                                                      : "controlling");
    absl::Format(&sink, " pqc: %u dtls_in_stun: %u ", config.pqc,
                 config.dtls_in_stun);
    sink.Append(" ]");
  }
};

struct TestConfig {
  int pct_loss = -1;
  int client_interface_count = -1;
  int server_interface_count = -1;

  bool client_ice_controller;
  SSLProtocolVersion protocol_version;
  EndpointConfig client_config;
  EndpointConfig server_config;

  TestConfig& fix() {
    if (client_ice_controller) {
      client_config.ice_role = ICEROLE_CONTROLLING;
      server_config.ice_role = ICEROLE_CONTROLLED;
    } else {
      client_config.ice_role = ICEROLE_CONTROLLED;
      server_config.ice_role = ICEROLE_CONTROLLING;
    }
    client_config.ssl_role = SSL_CLIENT;
    server_config.ssl_role = SSL_SERVER;
    client_config.max_protocol_version = protocol_version;
    server_config.max_protocol_version = protocol_version;
    return *this;
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const TestConfig& config) {
    if (config.pct_loss >= 0) {
      absl::Format(&sink, "loss: %u ", config.pct_loss);
    }
    if (config.server_interface_count >= 0) {
      absl::Format(&sink, "server_interface_count: %u ",
                   config.server_interface_count);
    }
    sink.Append("[ client: ");
    AbslStringify(sink, config.client_config);
    sink.Append("[ server: ");
    AbslStringify(sink, config.server_config);
    sink.Append("]");
  }

  static constexpr EndpointConfig kEndpointVariants[] = {
      {
          .dtls_in_stun = false,
          .pqc = false,
      },
      {
          .dtls_in_stun = true,
          .pqc = false,
      },
      {
          .dtls_in_stun = false,
          .pqc = true,
      },
      {
          .dtls_in_stun = true,
          .pqc = true,
      },
  };

  static std::vector<TestConfig> Variants() {
    std::vector<TestConfig> out;
    for (auto cc : kEndpointVariants) {
      for (auto sc : kEndpointVariants) {
        for (auto cic : {true, false}) {
          for (auto p : {SSL_PROTOCOL_DTLS_12, SSL_PROTOCOL_DTLS_13}) {
            auto config = TestConfig{.client_ice_controller = cic,
                                     .protocol_version = p,
                                     .client_config = cc,
                                     .server_config = sc}
                              .fix();
            if (config.client_config.max_protocol_version ==
                    SSL_PROTOCOL_DTLS_12 &&
                config.client_config.pqc) {
              continue;
            }
            if (config.server_config.max_protocol_version ==
                    SSL_PROTOCOL_DTLS_12 &&
                config.server_config.pqc) {
              continue;
            }
            out.push_back(config);
          }
        }
      }
    }
    return out;
  }
};

class DtlsIceIntegrationTest : public ::testing::TestWithParam<TestConfig> {
 public:
  void CandidateC2S(IceTransportInternal*, const Candidate& c) {
    server_thread()->PostTask(
        [this, c = c]() { server_.ice()->AddRemoteCandidate(c); });
  }
  void CandidateS2C(IceTransportInternal*, const Candidate& c) {
    client_thread()->PostTask(
        [this, c = c]() { client_.ice()->AddRemoteCandidate(c); });
  }

 private:
  struct Endpoint {
    explicit Endpoint(bool client_, const EndpointConfig& config_)
        : client(client_),
          config(config_),
          env(CreateEnvironment(CreateTestFieldTrialsPtr(
              config.dtls_in_stun ? "WebRTC-IceHandshakeDtls/Enabled/" : ""))) {
    }

    bool client;
    EmulatedNetworkManagerInterface* emulated_network_manager = nullptr;
    std::unique_ptr<NetworkManager> network_manager;
    std::unique_ptr<BasicPacketSocketFactory> packet_socket_factory;
    std::unique_ptr<PortAllocator> allocator;
    scoped_refptr<IceTransportInterface> ice_transport;
    std::unique_ptr<DtlsTransportInternalImpl> dtls;

    // Convenience getter for the internal transport.
    IceTransportInternal* ice() { return ice_transport->internal(); }

    // SetRemoteFingerprintFromCert does not actually set the fingerprint,
    // but only store it for setting later.
    bool store_but_dont_set_remote_fingerprint = false;
    std::unique_ptr<SSLFingerprint> remote_fingerprint;

    scoped_refptr<RTCCertificate> local_certificate;
    scoped_refptr<RTCCertificate> remote_certificate;

    EndpointConfig config;
    Environment env;

    void Restart(DtlsIceIntegrationTest& test) {
      dtls.reset();
      ice_transport = nullptr;
      allocator.reset();
      packet_socket_factory.reset();

      packet_socket_factory = std::make_unique<BasicPacketSocketFactory>(
          emulated_network_manager->socket_factory());
      allocator = std::make_unique<BasicPortAllocator>(
          env, network_manager.get(), packet_socket_factory.get());
      test.SetupIceAndDtls(*this);
      allocator->Initialize();
    }
  };

 protected:
  DtlsIceIntegrationTest()
      : ss_(std::make_unique<VirtualSocketServer>()),
        socket_factory_(std::make_unique<BasicPacketSocketFactory>(ss_.get())),
        client_(/* client= */ true, GetParam().client_config),
        server_(/* client= */ false, GetParam().server_config),
        client_ice_parameters_("c_ufrag",
                               "c_icepwd_something_something",
                               false),
        server_ice_parameters_("s_ufrag",
                               "s_icepwd_something_something",
                               false) {}

  void ConfigureEmulatedNetwork(int pct_loss = 50,
                                int client_interface_count = 1,
                                int server_interface_count = 1) {
    network_emulation_manager_ =
        CreateNetworkEmulationManager({.time_mode = TimeMode::kSimulated});

    BuiltInNetworkBehaviorConfig networkBehavior;
    networkBehavior.link_capacity = DataRate::KilobitsPerSec(220);
    networkBehavior.queue_delay_ms = 100;
    networkBehavior.queue_length_packets = 30;
    networkBehavior.loss_percent = pct_loss;

    auto pair = network_emulation_manager_->CreateEndpointPairWithTwoWayRoutes(
        networkBehavior, client_interface_count, server_interface_count);
    client_.emulated_network_manager = pair.first;
    server_.emulated_network_manager = pair.second;
  }

  void SetupEndpoint(Endpoint& ep,
                     const scoped_refptr<RTCCertificate> client_certificate,
                     const scoped_refptr<RTCCertificate> server_certificate) {
    thread(ep)->BlockingCall([&]() {
      if (!network_manager_) {
        network_manager_ =
            std::make_unique<FakeNetworkManager>(Thread::Current());
      }
      if (network_emulation_manager_ == nullptr) {
        ep.allocator = std::make_unique<BasicPortAllocator>(
            ep.env, network_manager_.get(), socket_factory_.get());
      } else {
        ep.network_manager =
            ep.emulated_network_manager->ReleaseNetworkManager();
        ep.packet_socket_factory = std::make_unique<BasicPacketSocketFactory>(
            ep.emulated_network_manager->socket_factory());
        ep.allocator = std::make_unique<BasicPortAllocator>(
            ep.env, ep.network_manager.get(), ep.packet_socket_factory.get());
      }
      ep.local_certificate =
          ep.client ? client_certificate : server_certificate;
      ep.remote_certificate =
          ep.client ? server_certificate : client_certificate;
      SetupIceAndDtls(ep);
    });
  }

  void SetupIceAndDtls(Endpoint& ep) {
    ep.allocator->set_flags(ep.allocator->flags() | PORTALLOCATOR_DISABLE_TCP);
    ep.ice_transport = make_ref_counted<FakeIceTransport>(
        std::make_unique<P2PTransportChannel>(
            ep.env, ep.client ? "client_transport" : "server_transport", 0,
            ep.allocator.get()));
    CryptoOptions crypto_options;
    if (ep.config.pqc) {
      FieldTrials field_trials("WebRTC-EnableDtlsPqc/Enabled/");
      crypto_options.ephemeral_key_exchange_cipher_groups.Update(&field_trials);
    }
    ep.dtls = std::make_unique<DtlsTransportInternalImpl>(
        ep.env, ep.ice_transport, crypto_options,
        ep.config.max_protocol_version);

    // Enable(or disable) the dtls_in_stun parameter before
    // DTLS is negotiated.
    IceConfig config;
    config.continual_gathering_policy = GATHER_CONTINUALLY;
    config.dtls_handshake_in_stun = ep.config.dtls_in_stun;
    ep.ice()->SetIceConfig(config);

    // Setup ICE.
    ep.ice()->SetIceParameters(ep.client ? client_ice_parameters_
                                         : server_ice_parameters_);
    ep.ice()->SetRemoteIceParameters(ep.client ? server_ice_parameters_
                                               : client_ice_parameters_);
    ep.ice()->SetIceRole(ep.config.ice_role);
    if (ep.client) {
      ep.ice()->SubscribeCandidateGathered(
          [this](IceTransportInternal* transport, const Candidate& candidate) {
            CandidateC2S(transport, candidate);
          });
    } else {
      ep.ice()->SubscribeCandidateGathered(
          [this](IceTransportInternal* transport, const Candidate& candidate) {
            CandidateS2C(transport, candidate);
          });
    }

    // Setup DTLS.
    ep.dtls->SetDtlsRole(ep.config.ssl_role);
    SetLocalCertificate(ep, ep.local_certificate);
    SetRemoteFingerprintFromCert(ep, ep.remote_certificate);
  }

  void Prepare() {
    auto client_certificate =
        RTCCertificate::Create(SSLIdentity::Create("test", KT_DEFAULT));
    auto server_certificate =
        RTCCertificate::Create(SSLIdentity::Create("test", KT_DEFAULT));

    if (network_emulation_manager_ == nullptr) {
      thread_ = std::make_unique<AutoSocketServerThread>(ss_.get());
    }

    client_thread()->BlockingCall([&]() {
      SetupEndpoint(client_, client_certificate, server_certificate);
    });

    server_thread()->BlockingCall([&]() {
      SetupEndpoint(server_, client_certificate, server_certificate);
    });

    // Setup the network.
    if (network_emulation_manager_ == nullptr) {
      network_manager_->AddInterface(SocketAddress("192.168.1.1", 0));
    }

    client_thread()->BlockingCall([&]() { client_.allocator->Initialize(); });
    server_thread()->BlockingCall([&]() { server_.allocator->Initialize(); });
  }

  void TearDown() override {
    if (client_thread() != nullptr) {
      client_thread()->BlockingCall([&]() {
        client_.dtls.reset();
        client_.ice_transport = nullptr;
        client_.allocator.reset();
      });
    }

    if (server_thread() != nullptr) {
      server_thread()->BlockingCall([&]() {
        server_.dtls.reset();
        server_.ice_transport = nullptr;
        server_.allocator.reset();
      });
    }
  }

  ~DtlsIceIntegrationTest() override = default;

  static int CountConnectionsWithFilter(
      IceTransportInternal* ice,
      std::function<bool(const ConnectionInfo&)> filter) {
    IceTransportStats stats;
    ice->GetStats(&stats);
    int count = 0;
    for (const auto& con : stats.connection_infos) {
      if (filter(con)) {
        count++;
      }
    }
    return count;
  }

  static int CountConnections(IceTransportInternal* ice) {
    return CountConnectionsWithFilter(ice, [](auto con) { return true; });
  }

  static int CountWritableConnections(IceTransportInternal* ice) {
    return CountConnectionsWithFilter(ice,
                                      [](auto con) { return con.writable; });
  }

  WaitUntilSettings wait_until_settings() {
    if (network_emulation_manager_ == nullptr) {
      return {
          .timeout = TimeDelta::Millis(kDefaultTimeout),
          .clock = &fake_clock_,
      };
    } else {
      return {
          .timeout = TimeDelta::Millis(kDefaultTimeout),
          .clock = network_emulation_manager_->time_controller(),
      };
    }
  }

  Thread* thread(Endpoint& ep) {
    if (ep.emulated_network_manager == nullptr) {
      return thread_.get();
    } else {
      return ep.emulated_network_manager->network_thread();
    }
  }

  Thread* client_thread() { return thread(client_); }

  Thread* server_thread() { return thread(server_); }

  void SetRemoteFingerprintFromCert(Endpoint& ep,
                                    const scoped_refptr<RTCCertificate>& cert) {
    ep.remote_fingerprint = SSLFingerprint::CreateFromCertificate(*cert);
    if (ep.store_but_dont_set_remote_fingerprint) {
      return;
    }
    SetRemoteFingerprint(ep);
  }

  void SetRemoteFingerprint(Endpoint& ep) {
    RTC_CHECK(ep.remote_fingerprint);
    RTC_LOG(LS_INFO) << ((&ep == &client_) ? "client" : "server")
                     << "::SetRemoteFingerprint";
    ep.dtls->SetRemoteParameters(
        ep.remote_fingerprint->algorithm,
        reinterpret_cast<const uint8_t*>(ep.remote_fingerprint->digest.data()),
        ep.remote_fingerprint->digest.size(), std::nullopt);
  }

  void SetLocalCertificate(Endpoint& ep,
                           const scoped_refptr<RTCCertificate> certificate) {
    RTC_CHECK(certificate);
    RTC_LOG(LS_INFO) << ((&ep == &client_) ? "client" : "server")
                     << "::SetLocalCertificate: ";
    ep.dtls->SetLocalCertificate(certificate);
  }

  ScopedFakeClock fake_clock_;
  std::unique_ptr<VirtualSocketServer> ss_;
  std::unique_ptr<BasicPacketSocketFactory> socket_factory_;
  std::unique_ptr<NetworkEmulationManager> network_emulation_manager_;
  std::unique_ptr<AutoSocketServerThread> thread_;
  std::unique_ptr<FakeNetworkManager> network_manager_;

  Endpoint client_;
  Endpoint server_;

  IceParameters client_ice_parameters_;
  IceParameters server_ice_parameters_;
};

TEST_P(DtlsIceIntegrationTest, SmokeTest) {
  Prepare();
  client_.ice()->MaybeStartGathering();
  server_.ice()->MaybeStartGathering();

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());
  EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);

  if (!(client_.config.pqc || server_.config.pqc) &&
      client_.config.dtls_in_stun && server_.config.dtls_in_stun) {
    EXPECT_EQ(client_.dtls->GetStunDataCount(), 1);
    EXPECT_EQ(server_.dtls->GetStunDataCount(), 2);
  } else {
    // TODO(webrtc:404763475)
  }

  EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
  EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);

  // Validate that we can add new Connections (that become writable).
  network_manager_->AddInterface(SocketAddress("192.168.2.1", 0));
  EXPECT_THAT(WaitUntil(
                  [&] {
                    return CountWritableConnections(client_.ice()) > 1 &&
                           CountWritableConnections(server_.ice()) > 1;
                  },
                  IsTrue(), wait_until_settings()),
              IsRtcOk());
}

// Check that DtlsInStun still works even if SetRemoteFingerprint is called
// "late". This is what happens if the answer sdp comes strictly after ICE has
// connected. Before this patch, this would disable stun-piggy-backing.
TEST_P(DtlsIceIntegrationTest, ClientLateCertificate) {
  client_.store_but_dont_set_remote_fingerprint = true;
  Prepare();
  client_.ice()->MaybeStartGathering();
  server_.ice()->MaybeStartGathering();

  ASSERT_THAT(
      WaitUntil([&] { return CountWritableConnections(client_.ice()) > 0; },
                IsTrue(), wait_until_settings()),
      IsRtcOk());
  SetRemoteFingerprint(client_);

  ASSERT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());

  EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);

  EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);

  EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
  EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);
}

TEST_P(DtlsIceIntegrationTest, TestWithPacketLoss) {
  if (!SSLStreamAdapter::IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }
  ConfigureEmulatedNetwork();
  Prepare();

  client_thread()->PostTask([&]() { client_.ice()->MaybeStartGathering(); });

  server_thread()->PostTask([&]() { server_.ice()->MaybeStartGathering(); });

  EXPECT_THAT(WaitUntil(
                  [&] {
                    return client_thread()->BlockingCall([&]() {
                      return client_.dtls->writable();
                    }) && server_thread()->BlockingCall([&]() {
                      return server_.dtls->writable();
                    });
                  },
                  IsTrue(), wait_until_settings()),
              IsRtcOk());

  EXPECT_EQ(client_thread()->BlockingCall([&]() {
    return client_.dtls->IsDtlsPiggybackSupportedByPeer();
  }),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_thread()->BlockingCall([&]() {
    return server_.dtls->IsDtlsPiggybackSupportedByPeer();
  }),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
}

TEST_P(DtlsIceIntegrationTest, LongRunningTestWithPacketLoss) {
  if (!SSLStreamAdapter::IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }
  int seed = absl::GetFlag(FLAGS_long_running_seed);
  if (seed == 0) {
    seed = 1 + time(0);
  }
  RTC_LOG(LS_INFO) << "seed: " << seed;
  Random rand(seed);
  ConfigureEmulatedNetwork();
  Prepare();

  client_thread()->PostTask([&]() { client_.ice()->MaybeStartGathering(); });

  server_thread()->PostTask([&]() { server_.ice()->MaybeStartGathering(); });

  ASSERT_THAT(WaitUntil(
                  [&] {
                    return client_thread()->BlockingCall([&]() {
                      return client_.dtls->writable();
                    }) && server_thread()->BlockingCall([&]() {
                      return server_.dtls->writable();
                    });
                  },
                  IsTrue(), wait_until_settings()),
              IsRtcOk());

  auto now =
      network_emulation_manager_->time_controller()->GetClock()->CurrentTime();
  auto end = now + TimeDelta::Minutes(
                       absl::GetFlag(FLAGS_long_running_run_time_minutes));
  int client_sent = 0;
  int client_recv = 0;
  int server_sent = 0;
  int server_recv = 0;
  void* id = this;
  client_thread()->BlockingCall([&]() {
    return client_.dtls->RegisterReceivedPacketCallback(
        id, [&](auto, auto) { client_recv++; });
  });
  server_thread()->BlockingCall([&]() {
    return server_.dtls->RegisterReceivedPacketCallback(
        id, [&](auto, auto) { server_recv++; });
  });
  while (now < end) {
    int delay = static_cast<int>(rand.Gaussian(100, 25));
    if (delay < 25) {
      delay = 25;
    }
    network_emulation_manager_->time_controller()->AdvanceTime(
        TimeDelta::Millis(delay));
    now = network_emulation_manager_->time_controller()
              ->GetClock()
              ->CurrentTime();

    if (absl::GetFlag(FLAGS_long_running_send_data)) {
      int flags = 0;
      AsyncSocketPacketOptions options;
      std::string a_long_string(500, 'a');
      if (client_thread()->BlockingCall([&]() {
            return client_.dtls->SendPacket(
                a_long_string.c_str(), a_long_string.length(), options, flags);
          }) > 0) {
        client_sent++;
      }
      if (server_thread()->BlockingCall([&]() {
            return server_.dtls->SendPacket(
                a_long_string.c_str(), a_long_string.length(), options, flags);
          }) > 0) {
        server_sent++;
      }
    }

    EXPECT_THAT(WaitUntil(
                    [&] {
                      return client_thread()->BlockingCall([&]() {
                        return client_.dtls->writable();
                      }) && server_thread()->BlockingCall([&]() {
                        return server_.dtls->writable();
                      });
                    },
                    IsTrue(), wait_until_settings()),
                IsRtcOk());
    ASSERT_THAT(client_thread()->BlockingCall(
                    [&]() { return client_.dtls->dtls_state(); }),
                Not(Eq(DtlsTransportState::kFailed)));
    ASSERT_THAT(server_thread()->BlockingCall(
                    [&]() { return server_.dtls->dtls_state(); }),
                Not(Eq(DtlsTransportState::kFailed)));
  }

  client_thread()->BlockingCall(
      [&]() { return client_.dtls->DeregisterReceivedPacketCallback(id); });
  server_thread()->BlockingCall(
      [&]() { return server_.dtls->DeregisterReceivedPacketCallback(id); });

  RTC_LOG(LS_INFO) << "Server sent " << server_sent << " packets "
                   << " client received: " << client_recv << " ("
                   << (client_recv * 100 / (1 + server_sent)) << "%)";
  RTC_LOG(LS_INFO) << "Client sent " << client_sent << " packets "
                   << " server received: " << server_recv << " ("
                   << (server_recv * 100 / (1 + client_sent)) << "%)";
}

// Verify that DtlsStunPiggybacking works even if one (or several)
// of the STUN_BINDING_REQUESTs are so full that dtls does not fit.
TEST_P(DtlsIceIntegrationTest, AlmostFullSTUN_BINDING) {
  Prepare();

  std::string a_long_string(500, 'a');
  client_.ice()->GetDictionaryWriter()->get().SetByteString(77)->CopyBytes(
      a_long_string);
  server_.ice()->GetDictionaryWriter()->get().SetByteString(78)->CopyBytes(
      a_long_string);

  client_.ice()->MaybeStartGathering();
  server_.ice()->MaybeStartGathering();

  // Note: this only reaches the pending piggybacking state.
  EXPECT_THAT(
      WaitUntil(
          [&] { return client_.dtls->writable() && server_.dtls->writable(); },
          IsTrue(), wait_until_settings()),
      IsRtcOk());
  EXPECT_EQ(client_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_.dtls->IsDtlsPiggybackSupportedByPeer(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(client_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);
  EXPECT_EQ(server_.dtls->WasDtlsCompletedByPiggybacking(),
            client_.config.dtls_in_stun && server_.config.dtls_in_stun);

  if (!(client_.config.pqc || server_.config.pqc) &&
      client_.config.dtls_in_stun && server_.config.dtls_in_stun) {
    EXPECT_EQ(client_.dtls->GetStunDataCount(), 1);
    EXPECT_EQ(server_.dtls->GetStunDataCount(), 2);
  } else {
    // TODO(webrtc:404763475)
  }

  EXPECT_EQ(client_.dtls->GetRetransmissionCount(), 0);
  EXPECT_EQ(server_.dtls->GetRetransmissionCount(), 0);
}

// Test cases are parametrized by
// * client-piggybacking-enabled,
// * server-piggybacking-enabled,
// * maximum DTLS version to use.
INSTANTIATE_TEST_SUITE_P(DtlsStunPiggybackingIntegrationTest,
                         DtlsIceIntegrationTest,
                         ::testing::ValuesIn(TestConfig::Variants()));

struct DtlsIceIntegrationBenchmark : public DtlsIceIntegrationTest {
  static std::vector<TestConfig> Variants() {
    std::vector<TestConfig> out;
    for (auto loss : {0, 5, 10, 15, 25, 50}) {
      for (auto sif : {1, 2}) {
        for (auto cc : TestConfig::kEndpointVariants) {
          for (auto sc : TestConfig::kEndpointVariants) {
            for (auto cic : {true}) {
              for (auto p : {SSL_PROTOCOL_DTLS_12, SSL_PROTOCOL_DTLS_13}) {
                auto config = TestConfig{.pct_loss = loss,
                                         .client_interface_count = 1,
                                         .server_interface_count = sif,
                                         .client_ice_controller = cic,
                                         .protocol_version = p,
                                         .client_config = cc,
                                         .server_config = sc}
                                  .fix();
                if (config.client_config.max_protocol_version ==
                        SSL_PROTOCOL_DTLS_12 &&
                    config.client_config.pqc) {
                  continue;
                }
                if (config.server_config.max_protocol_version ==
                        SSL_PROTOCOL_DTLS_12 &&
                    config.server_config.pqc) {
                  continue;
                }
                if (config.client_config.pqc !=
                    config.client_config.dtls_in_stun) {
                  continue;
                }
                if (config.server_config.pqc !=
                    config.server_config.dtls_in_stun) {
                  continue;
                }
                if (config.client_config.pqc != config.server_config.pqc) {
                  continue;
                }
                out.push_back(config);
              }
            }
          }
        }
      }
    }
    return out;
  }
};

INSTANTIATE_TEST_SUITE_P(
    DtlsIceIntegrationBenchmark,
    DtlsIceIntegrationBenchmark,
    ::testing::ValuesIn(DtlsIceIntegrationBenchmark::Variants()));

TEST_P(DtlsIceIntegrationBenchmark, Benchmark) {
  if (!SSLStreamAdapter::IsBoringSsl()) {
    GTEST_SKIP() << "Needs boringssl.";
  }

  const int iter = absl::GetFlag(FLAGS_bench_iterations);
  if (iter == 0) {
    GTEST_SKIP() << "SKIP " << GetParam()
                 << " - filtered by cmd line argument.";
  }

  auto pct_loss_filter = ToIntSet(absl::GetFlag(FLAGS_bench_pct_loss));
  if (!pct_loss_filter.empty() &&
      !pct_loss_filter.contains(GetParam().pct_loss)) {
    GTEST_SKIP() << "SKIP " << GetParam()
                 << " - filtered by cmd line argument.";
  }

  auto server_candidates_filter =
      ToIntSet(absl::GetFlag(FLAGS_bench_server_candidates));
  if (!server_candidates_filter.empty() &&
      !server_candidates_filter.contains(GetParam().server_interface_count)) {
    GTEST_SKIP() << "SKIP " << GetParam()
                 << " - filtered by cmd line argument.";
  }

  RTC_LOG(LS_INFO) << GetParam() << " START";

  ConfigureEmulatedNetwork(GetParam().pct_loss,
                           GetParam().client_interface_count,
                           GetParam().server_interface_count);
  Prepare();

  SamplesStatsCounter stats(iter);
  for (int i = 0; i < iter; i++) {
    int client_sent = 0;
    std::atomic<int> client_recv = 0;
    int server_sent = 0;
    std::atomic<int> server_recv = 0;
    void* id = this;

    client_thread()->BlockingCall([&]() {
      return client_.dtls->RegisterReceivedPacketCallback(
          id, [&](auto, auto) { client_recv++; });
    });
    server_thread()->BlockingCall([&]() {
      return server_.dtls->RegisterReceivedPacketCallback(
          id, [&](auto, auto) { server_recv++; });
    });

    client_thread()->PostTask([&]() { client_.ice()->MaybeStartGathering(); });
    server_thread()->PostTask([&]() { server_.ice()->MaybeStartGathering(); });

    auto start = network_emulation_manager_->time_controller()
                     ->GetClock()
                     ->CurrentTime();

    while (client_recv == 0 || server_recv == 0) {
      int delay = 50;
      network_emulation_manager_->time_controller()->AdvanceTime(
          TimeDelta::Millis(delay));

      // Send data
      {
        int flags = 0;
        AsyncSocketPacketOptions options;
        std::string a_string(50, 'a');

        if (client_.dtls->writable()) {
          client_thread()->BlockingCall([&]() {
            if (client_.dtls->SendPacket(a_string.c_str(), a_string.length(),
                                         options, flags) > 0) {
              client_sent++;
            }
          });
        }
        if (server_.dtls->writable()) {
          server_thread()->BlockingCall([&]() {
            if (server_.dtls->SendPacket(a_string.c_str(), a_string.length(),
                                         options, flags) > 0) {
              server_sent++;
            }
          });
        }
      }
    }
    auto end = network_emulation_manager_->time_controller()
                   ->GetClock()
                   ->CurrentTime();
    stats.AddSample(SamplesStatsCounter::StatsSample{
        .value = static_cast<double>((end - start).ms()),
        .time = end,
    });
    client_thread()->BlockingCall(
        [&]() { return client_.dtls->DeregisterReceivedPacketCallback(id); });
    server_thread()->BlockingCall(
        [&]() { return server_.dtls->DeregisterReceivedPacketCallback(id); });
    client_thread()->BlockingCall([&]() { client_.Restart(*this); });
    server_thread()->BlockingCall([&]() { server_.Restart(*this); });
  }
  RTC_LOG(LS_INFO) << GetParam() << " RESULT:"
                   << " p10: " << stats.GetPercentile(0.10)
                   << " p50: " << stats.GetPercentile(0.50)
                   << " avg: " << stats.GetAverage()
                   << " p95: " << stats.GetPercentile(0.95);
}

}  // namespace
}  // namespace webrtc
