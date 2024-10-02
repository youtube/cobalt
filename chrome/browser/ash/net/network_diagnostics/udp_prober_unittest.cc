// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/udp_prober.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_host_resolver.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_network_context.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace network_diagnostics {

namespace {

using ProbeExitEnum = UdpProber::ProbeExitEnum;

}  // namespace

class UdpProberWithFakeNetworkContextTest : public ::testing::Test {
 public:
  UdpProberWithFakeNetworkContextTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  UdpProberWithFakeNetworkContextTest(
      const UdpProberWithFakeNetworkContextTest&) = delete;
  UdpProberWithFakeNetworkContextTest& operator=(
      const UdpProberWithFakeNetworkContextTest&) = delete;

  void InitializeProberNetworkContext(
      std::unique_ptr<FakeHostResolver::DnsResult> fake_dns_result,
      absl::optional<net::Error> udp_connect_complete_code,
      absl::optional<net::Error> udp_send_complete_code,
      absl::optional<net::Error> udp_on_received_code,
      absl::optional<base::span<const uint8_t>> udp_on_received_data) {
    fake_network_context_ = std::make_unique<FakeNetworkContext>();
    fake_network_context_->set_fake_dns_result(std::move(fake_dns_result));
    if (udp_connect_complete_code.has_value()) {
      fake_network_context_->SetUdpConnectCode(
          udp_connect_complete_code.value());
    }
    if (udp_send_complete_code.has_value()) {
      fake_network_context_->SetUdpSendCode(udp_send_complete_code.value());
    }
    if (udp_on_received_code.has_value()) {
      fake_network_context_->SetUdpOnReceivedCode(udp_on_received_code.value());
    }
    if (udp_on_received_data.has_value()) {
      fake_network_context_->SetUdpOnReceivedData(
          std::move(udp_on_received_data.value()));
    }
  }

  void SetUdpDelays(absl::optional<base::TimeDelta> connection_delay,
                    absl::optional<base::TimeDelta> send_delay,
                    absl::optional<base::TimeDelta> receive_delay) {
    fake_network_context_->SetTaskEnvironmentForTesting(&task_environment_);
    if (connection_delay.has_value()) {
      fake_network_context_->SetUdpConnectionDelay(connection_delay.value());
    }
    if (send_delay.has_value()) {
      fake_network_context_->SetUdpSendDelay(send_delay.value());
    }
    if (receive_delay.has_value()) {
      fake_network_context_->SetUdpReceiveDelay(receive_delay.value());
    }
  }

  void CreateAndExecuteUdpProber(base::span<const uint8_t> data,
                                 UdpProber::UdpProbeCompleteCallback callback) {
    ASSERT_TRUE(fake_network_context_);
    udp_prober_ = UdpProber::Start(
        base::BindRepeating(
            [](network::mojom::NetworkContext* network_context) {
              return network_context;
            },
            fake_network_context_.get()),
        kFakeHostPortPair, std::move(data), kStunTag,
        kTimeoutAfterHostResolution, std::move(callback));
  }

  void RunProberExpectingResult(int expected_result,
                                ProbeExitEnum expected_exit_enum) {
    int probe_result = -1;
    ProbeExitEnum probe_exit_enum = ProbeExitEnum::kConnectFailure;
    base::RunLoop run_loop;
    CreateAndExecuteUdpProber(
        kValidStunData,
        base::BindLambdaForTesting([&](int result, ProbeExitEnum exit_enum) {
          probe_result = result;
          probe_exit_enum = exit_enum;
          EXPECT_EQ(expected_result, probe_result);
          EXPECT_EQ(expected_exit_enum, probe_exit_enum);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  FakeNetworkContext* fake_network_context() {
    return fake_network_context_.get();
  }

 protected:
  const net::HostPortPair kFakeHostPortPair =
      net::HostPortPair::FromString("fake_stun_server.com:80");
  const net::IPEndPoint kFakeIPAddress{
      net::IPEndPoint(net::IPAddress::IPv4Localhost(), /*port=*/1234)};
  const base::span<const uint8_t> kValidStunData = util::GetStunHeader();
  const net::NetworkTrafficAnnotationTag kStunTag =
      util::GetStunNetworkAnnotationTag();
  const base::TimeDelta kTimeoutAfterHostResolution = base::Seconds(10);

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeNetworkContext> fake_network_context_;
  std::unique_ptr<UdpProber> udp_prober_;
};

TEST_F(UdpProberWithFakeNetworkContextTest, SuccessfulEndToEndResponse) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  std::array<uint8_t, 1> udp_on_received_data = {0x00};
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/net::OK,
                                 /*udp_send_complete_code=*/net::OK,
                                 /*udp_on_received_code=*/net::OK,
                                 udp_on_received_data);
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/base::Seconds(1),
               /*receive_delay=*/base::Seconds(1));
  RunProberExpectingResult(net::OK, ProbeExitEnum::kSuccess);
}

TEST_F(UdpProberWithFakeNetworkContextTest, FailedDnsLookup) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::ERR_NAME_NOT_RESOLVED,
      net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED), net::AddressList(),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  // UDP connect and subsequent steps will not happen in this scenario.
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/absl::nullopt,
                                 /*udp_send_complete_code=*/absl::nullopt,
                                 /*udp_on_received_code=*/absl::nullopt,
                                 /*udp_on_received_data=*/absl::nullopt);
  RunProberExpectingResult(net::ERR_NAME_NOT_RESOLVED,
                           ProbeExitEnum::kDnsFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, MojoDisconnectDnsLookup) {
  // UDP connect and subsequent steps will not happen in this scenario.
  InitializeProberNetworkContext(/*fake_dns_result=*/{},
                                 /*udp_connect_code=*/absl::nullopt,
                                 /*udp_send_code=*/absl::nullopt,
                                 /*udp_on_received_code=*/absl::nullopt,
                                 /*udp_on_received_data=*/absl::nullopt);
  fake_network_context()->set_disconnect_during_host_resolution(true);
  RunProberExpectingResult(net::ERR_NAME_NOT_RESOLVED,
                           ProbeExitEnum::kDnsFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, FailedUdpConnection) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(
      std::move(fake_dns_result),
      /*udp_connect_code=*/net::ERR_CONNECTION_FAILED,
      /*udp_send_code=*/absl::nullopt,
      /*udp_on_received_code=*/absl::nullopt,
      /*udp_on_received_data=*/absl::nullopt);
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/absl::nullopt,
               /*receive_delay=*/absl::nullopt);
  RunProberExpectingResult(net::ERR_CONNECTION_FAILED,
                           ProbeExitEnum::kConnectFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, MojoDisconnectDuringUdpConnection) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/net::OK,
                                 /*udp_send_code=*/absl::nullopt,
                                 /*udp_on_received_code=*/absl::nullopt,
                                 /*udp_on_received_data=*/absl::nullopt);
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/absl::nullopt,
               /*receive_delay=*/absl::nullopt);
  fake_network_context()->set_disconnect_during_udp_connection_attempt(true);
  RunProberExpectingResult(net::ERR_FAILED,
                           ProbeExitEnum::kMojoDisconnectFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, FailedUdpSend) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/net::OK,
                                 /*udp_send_code=*/net::ERR_CONNECTION_FAILED,
                                 /*udp_on_received_code=*/absl::nullopt,
                                 /*udp_on_received_data=*/absl::nullopt);
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/base::Seconds(1),
               /*receive_delay=*/absl::nullopt);
  RunProberExpectingResult(net::ERR_CONNECTION_FAILED,
                           ProbeExitEnum::kSendFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, MojoDisconnectDuringUdpSend) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  net::Error udp_connect_code = net::OK;
  InitializeProberNetworkContext(std::move(fake_dns_result), udp_connect_code,
                                 /*udp_send_code=*/absl::nullopt,
                                 /*udp_on_received_code=*/absl::nullopt,
                                 /*udp_on_received_data=*/absl::nullopt);
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/base::Seconds(1),
               /*receive_delay=*/absl::nullopt);
  fake_network_context()->SetDisconnectDuringUdpSendAttempt(true);
  RunProberExpectingResult(net::ERR_FAILED,
                           ProbeExitEnum::kMojoDisconnectFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, BadUdpNetworkCodeOnReceive) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(
      std::move(fake_dns_result),
      /*udp_connect_code=*/net::OK,
      /*udp_send_code=*/net::OK,
      /*udp_on_received_code=*/net::ERR_CONNECTION_FAILED,
      /*udp_on_received_data=*/absl::nullopt);
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/base::Seconds(1),
               /*receive_delay=*/base::Seconds(1));
  RunProberExpectingResult(net::ERR_CONNECTION_FAILED,
                           ProbeExitEnum::kNetworkErrorOnReceiveFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, NoDataReceivedOnReceiveFailure) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/net::OK,
                                 /*udp_send_code=*/net::OK,
                                 /*udp_on_received_code*/ net::OK,
                                 /*udp_on_received_data=*/{});
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/base::Seconds(1),
               /*receive_delay=*/base::Seconds(1));
  RunProberExpectingResult(net::ERR_FAILED,
                           ProbeExitEnum::kNoDataReceivedFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, MojoDisconnectDuringUdpReceive) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/net::OK,
                                 /*udp_send_code=*/net::OK,
                                 /*udp_on_received_code=*/net::OK,
                                 /*udp_on_received_data=*/{});
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/base::Seconds(1),
               /*receive_delay=*/base::Seconds(1));
  fake_network_context()->SetDisconnectDuringUdpReceiveAttempt(true);
  RunProberExpectingResult(net::ERR_FAILED,
                           ProbeExitEnum::kMojoDisconnectFailure);
}

TEST_F(UdpProberWithFakeNetworkContextTest, ProbeTimeoutDuringUdpConnection) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/net::OK,
                                 /*udp_send_complete_code=*/absl::nullopt,
                                 /*udp_on_received_code=*/absl::nullopt,
                                 /*udp_on_received_data=*/{});
  SetUdpDelays(/*connection_delay=*/base::Seconds(15),
               /*send_delay=*/absl::nullopt,
               /*receive_delay=*/absl::nullopt);
  RunProberExpectingResult(net::ERR_TIMED_OUT, ProbeExitEnum::kTimeout);
}

TEST_F(UdpProberWithFakeNetworkContextTest, ProbeTimeoutDuringUdpSend) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/net::OK,
                                 /*udp_send_complete_code=*/net::OK,
                                 /*udp_on_received_code=*/absl::nullopt,
                                 /*udp_on_received_data=*/{});
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/base::Seconds(15),
               /*receive_delay=*/absl::nullopt);
  RunProberExpectingResult(net::ERR_TIMED_OUT, ProbeExitEnum::kTimeout);
}

TEST_F(UdpProberWithFakeNetworkContextTest, ProbeTimeoutDuringUdpReceive) {
  auto fake_dns_result = std::make_unique<FakeHostResolver::DnsResult>(
      net::OK, net::ResolveErrorInfo(net::OK), net::AddressList(kFakeIPAddress),
      /*endpoint_results_with_metadata=*/absl::nullopt);
  InitializeProberNetworkContext(std::move(fake_dns_result),
                                 /*udp_connect_code=*/net::OK,
                                 /*udp_send_complete_code=*/net::OK,
                                 /*udp_on_received_code=*/net::OK,
                                 /*udp_on_received_data=*/{});
  SetUdpDelays(/*connection_delay=*/base::Seconds(1),
               /*send_delay=*/base::Seconds(1),
               /*receive_delay=*/base::Seconds(15));
  RunProberExpectingResult(net::ERR_TIMED_OUT, ProbeExitEnum::kTimeout);
}

}  // namespace network_diagnostics
}  // namespace ash
