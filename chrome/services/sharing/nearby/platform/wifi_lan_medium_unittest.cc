// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_lan_medium.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/services/sharing/nearby/platform/wifi_lan_server_socket.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole_factory.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_socket_factory.h"
#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/nearby/src/internal/platform/nsd_service_info.h"

namespace nearby {
namespace chrome {

namespace {

const char kLocalIpString[] = "\xC0\xA8\x56\x4B";
const int kLocalPort = ash::nearby::TcpServerSocketPort::kMin;
const net::IPEndPoint kLocalAddress(net::IPAddress(192, 168, 86, 75),
                                    kLocalPort);

const char kRemoteIpString[] = "\xC0\xA8\x56\x3E";
const int kRemotePort = ash::nearby::TcpServerSocketPort::kMax;

const char kIPv4ConfigPath[] = "/ipconfig/ipv4_config";
const char kWifiGuid[] = "wifi_guid";
const char kWifiServiceName[] = "wifi_service_name";
const char kWifiServicePath[] = "/service/wifi0";

}  // namespace

class WifiLanMediumTest : public ::testing::Test {
 public:
  enum class WifiInitState {
    // Network fully set up with valid IP address.
    kComplete,

    // IP address is invalid, e.g., a loopback address.
    kIpAddressInvalid,

    // Network is set up, but IP configs will be empty. In other words,
    // CrosNetworkConfig::GetManagedProperties() will succeed but no ip_configs
    // will be returned.
    kNoIpConfigs,

    // CrosNetworkConfig::GetManagedProperties() will fail.
    kNoManagedProperties,

    // There is no registered Wi-Fi network. In other words,
    // CrosNetworkConfig::GetNetworkStateList() will return an empty list.
    kNoWifiService
  };

  WifiLanMediumTest() = default;
  ~WifiLanMediumTest() override = default;
  WifiLanMediumTest(const WifiLanMediumTest&) = delete;
  WifiLanMediumTest& operator=(const WifiLanMediumTest&) = delete;

  void Initialize(WifiInitState state) {
    // Set up TCP socket factory mojo service.
    auto fake_socket_factory =
        std::make_unique<ash::nearby::FakeTcpSocketFactory>(
            /*default_local_addr=*/kLocalAddress);
    fake_socket_factory_ = fake_socket_factory.get();
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_socket_factory),
        socket_factory_shared_remote_.BindNewPipeAndPassReceiver());

    // Sets up a test Wi-Fi network to varying degrees depending on |state|.
    // This is needed in order to fetch the local IP address during server
    // socket creation.
    // TODO(b/278643115) Remove LoginState dependency.
    ash::LoginState::Initialize();

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());

    switch (state) {
      case WifiInitState::kComplete:
        InitializeCrosNetworkConfig(/*use_managed_config_handler=*/true);
        AddWifiService(/*add_ip_configs=*/true, kLocalAddress.address());
        break;
      case WifiInitState::kIpAddressInvalid:
        InitializeCrosNetworkConfig(/*use_managed_config_handler=*/true);
        AddWifiService(/*add_ip_configs=*/true,
                       net::IPAddress::IPv4Localhost());
        break;
      case WifiInitState::kNoIpConfigs:
        InitializeCrosNetworkConfig(/*use_managed_config_handler=*/true);
        AddWifiService(/*add_ip_configs=*/false, kLocalAddress.address());
        break;
      case WifiInitState::kNoManagedProperties:
        InitializeCrosNetworkConfig(/*use_managed_config_handler=*/false);
        AddWifiService(/*add_ip_configs=*/true, kLocalAddress.address());
        break;
      case WifiInitState::kNoWifiService:
        InitializeCrosNetworkConfig(/*use_managed_config_handler=*/false);
        break;
    }

    // Set up firewall hole factory mojo service.
    auto fake_firewall_hole_factory =
        std::make_unique<ash::nearby::FakeFirewallHoleFactory>();
    fake_firewall_hole_factory_ = fake_firewall_hole_factory.get();
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_firewall_hole_factory),
        firewall_hole_factory_shared_remote_.BindNewPipeAndPassReceiver());

    nsd_service_info_.SetIPAddress(kRemoteIpString);
    nsd_service_info_.SetPort(kRemotePort);

    wifi_lan_medium_ = std::make_unique<WifiLanMedium>(
        socket_factory_shared_remote_, cros_network_config_,
        firewall_hole_factory_shared_remote_);
  }

  void TearDown() override {
    wifi_lan_medium_.reset();
    cros_network_config_helper_.reset();
    managed_network_config_handler_.reset();
    ui_proxy_config_service_.reset();
    network_configuration_handler_.reset();
    network_profile_handler_.reset();
    scoped_user_manager_.reset();
    ash::LoginState::Shutdown();
  }

  // Calls ConnectToService()/ListenForService() from |num_threads|, which will
  // each block until failure or the TCP connected/server socket is created.
  // This method returns when |expected_num_calls_sent_to_socket_factory|
  // TcpSocketFactory::CreateTCPConnectedSocket()/CreateTCPServerSocket() calls
  // are queued up. When the ConnectToService()/ListenForService() calls finish
  // on all threads, |on_finished| is invoked.
  void CallConnectToServiceFromThreads(
      size_t num_threads,
      size_t expected_num_calls_sent_to_socket_factory,
      bool expected_success,
      base::OnceClosure on_connect_calls_finished,
      CancellationFlag* cancellation_flag = nullptr) {
    // The run loop quits when TcpSocketFactory receives all of the expected
    // CreateTCPConnectedSocket() calls.
    base::RunLoop run_loop;
    fake_socket_factory_->SetCreateConnectedSocketCallExpectations(
        expected_num_calls_sent_to_socket_factory,
        /*on_all_create_connected_socket_calls_queued=*/run_loop.QuitClosure());

    on_connect_calls_finished_ = std::move(on_connect_calls_finished);
    num_running_connect_calls_ = num_threads;
    for (size_t thread = 0; thread < num_threads; ++thread) {
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&WifiLanMediumTest::CallConnect,
                                    base::Unretained(this), expected_success,
                                    cancellation_flag));
    }
    run_loop.Run();
  }

  void CallListenForServiceFromThreads(
      size_t num_threads,
      size_t expected_num_calls_sent_to_socket_factory,
      bool expected_success,
      base::OnceClosure on_listen_calls_finished) {
    // The run loop quits when TcpSocketFactory receives all of the expected
    // CreateTCPServerSocket() calls.
    base::RunLoop run_loop;
    fake_socket_factory_->SetCreateServerSocketCallExpectations(
        expected_num_calls_sent_to_socket_factory,
        /*on_all_create_server_socket_calls_queued=*/run_loop.QuitClosure());

    on_listen_calls_finished_ = std::move(on_listen_calls_finished);
    num_running_listen_calls_ = num_threads;
    for (size_t thread = 0; thread < num_threads; ++thread) {
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&WifiLanMediumTest::CallListen,
                                    base::Unretained(this), expected_success));
    }
    run_loop.Run();
  }

 protected:
  // Boiler plate to set up a test CrosNetworkConfig mojo service.
  void InitializeCrosNetworkConfig(bool use_managed_config_handler) {
    cros_network_config_helper_ =
        std::make_unique<ash::network_config::CrosNetworkConfigTestHelper>(
            /*initialize=*/false);

    if (use_managed_config_handler) {
      network_profile_handler_ =
          ash::NetworkProfileHandler::InitializeForTesting();

      network_configuration_handler_ =
          ash::NetworkConfigurationHandler::InitializeForTest(
              cros_network_config_helper_->network_state_helper()
                  .network_state_handler(),
              cros_network_config_helper_->network_device_handler());

      PrefProxyConfigTrackerImpl::RegisterProfilePrefs(user_prefs_.registry());
      PrefProxyConfigTrackerImpl::RegisterPrefs(local_state_.registry());
      ::onc::RegisterProfilePrefs(user_prefs_.registry());
      ::onc::RegisterPrefs(local_state_.registry());

      ui_proxy_config_service_ = std::make_unique<ash::UIProxyConfigService>(
          &user_prefs_, &local_state_,
          cros_network_config_helper_->network_state_helper()
              .network_state_handler(),
          network_profile_handler_.get());

      managed_network_config_handler_ =
          ash::ManagedNetworkConfigurationHandler::InitializeForTesting(
              cros_network_config_helper_->network_state_helper()
                  .network_state_handler(),
              network_profile_handler_.get(),
              cros_network_config_helper_->network_device_handler(),
              network_configuration_handler_.get(),
              ui_proxy_config_service_.get());
      managed_network_config_handler_->SetPolicy(
          ::onc::ONC_SOURCE_DEVICE_POLICY,
          /*userhash=*/std::string(),
          /*network_configs_onc=*/base::Value::List(),
          /*global_network_config=*/base::Value::Dict());

      base::RunLoop().RunUntilIdle();
    }

    cros_network_config_helper_->Initialize(
        managed_network_config_handler_.get());
    cros_network_config_helper_->network_state_helper().ClearDevices();
    cros_network_config_helper_->network_state_helper().ClearServices();

    ash::network_config::BindToInProcessInstance(
        cros_network_config_.BindNewPipeAndPassReceiver());

    base::RunLoop().RunUntilIdle();
  }

  void AddWifiService(bool add_ip_configs, const net::IPAddress& local_addr) {
    if (add_ip_configs) {
      base::Value::Dict ipv4;
      ipv4.Set(shill::kAddressProperty, local_addr.ToString());
      ipv4.Set(shill::kMethodProperty, shill::kTypeIPv4);
      cros_network_config_helper_->network_state_helper()
          .ip_config_test()
          ->AddIPConfig(kIPv4ConfigPath, std::move(ipv4));
      base::RunLoop().RunUntilIdle();
    }

    cros_network_config_helper_->network_state_helper()
        .service_test()
        ->AddServiceWithIPConfig(kWifiServicePath, kWifiGuid, kWifiServiceName,
                                 shill::kTypeWifi, shill::kStateOnline,
                                 kIPv4ConfigPath, /*visible=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void CallConnect(bool expected_success, CancellationFlag* cancellation_flag) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow;
    std::unique_ptr<api::WifiLanSocket> connected_socket =
        wifi_lan_medium_->ConnectToService(
            /*remote_service_info=*/nsd_service_info_,
            /*cancellation_flag=*/cancellation_flag);

    ASSERT_EQ(expected_success, connected_socket != nullptr);
    if (--num_running_connect_calls_ == 0) {
      std::move(on_connect_calls_finished_).Run();
    }
  }

  void CallListen(bool expected_success) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow;
    std::unique_ptr<api::WifiLanServerSocket> server_socket =
        wifi_lan_medium_->ListenForService(/*port=*/kLocalPort);

    ASSERT_EQ(expected_success, server_socket != nullptr);
    if (expected_success) {
      // Verify that the server socket has the expected local IP:port.
      EXPECT_EQ(kLocalIpString, server_socket->GetIPAddress());
      EXPECT_EQ(kLocalPort, server_socket->GetPort());
    }

    if (--num_running_listen_calls_ == 0) {
      std::move(on_listen_calls_finished_).Run();
    }
  }

  base::test::TaskEnvironment task_environment_;
  size_t num_running_connect_calls_ = 0;
  size_t num_running_listen_calls_ = 0;
  base::OnceClosure on_connect_calls_finished_;
  base::OnceClosure on_listen_calls_finished_;

  // TCP socket factory:
  raw_ptr<ash::nearby::FakeTcpSocketFactory, ExperimentalAsh>
      fake_socket_factory_;
  mojo::SharedRemote<sharing::mojom::TcpSocketFactory>
      socket_factory_shared_remote_;

  // Local IP fetching:
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ash::NetworkProfileHandler> network_profile_handler_;
  std::unique_ptr<ash::NetworkConfigurationHandler>
      network_configuration_handler_;
  std::unique_ptr<ash::UIProxyConfigService> ui_proxy_config_service_;
  std::unique_ptr<ash::ManagedNetworkConfigurationHandler>
      managed_network_config_handler_;
  std::unique_ptr<ash::network_config::CrosNetworkConfigTestHelper>
      cros_network_config_helper_;
  mojo::SharedRemote<chromeos::network_config::mojom::CrosNetworkConfig>
      cros_network_config_;
  NsdServiceInfo nsd_service_info_;

  // Firewall hole factory:
  raw_ptr<ash::nearby::FakeFirewallHoleFactory, ExperimentalAsh>
      fake_firewall_hole_factory_;
  mojo::SharedRemote<sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory_shared_remote_;

  std::unique_ptr<WifiLanMedium> wifi_lan_medium_;
};

/*============================================================================*/
// Begin: ConnectToService()
/*============================================================================*/
TEST_F(WifiLanMediumTest, Connect_Success) {
  Initialize(WifiInitState::kComplete);

  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/1u,
      /*expected_success=*/true,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());
  fake_socket_factory_->FinishNextCreateConnectedSocket(net::OK);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_Cancelled) {
  Initialize(WifiInitState::kComplete);

  auto flag = std::make_unique<CancellationFlag>();
  flag->Cancel();
  CallConnect(
      /*expected_success=*/false,
      /*cancellation_flag=*/flag.get());
}

TEST_F(WifiLanMediumTest, Connect_CancelledDuringCall) {
  Initialize(WifiInitState::kComplete);

  auto flag = std::make_unique<CancellationFlag>();
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/1u,
      /*expected_success=*/false,
      /*on_connect_calls_finished=*/run_loop.QuitClosure(),
      /*cancellation_flag=*/flag.get());
  fake_socket_factory_->FinishNextCreateConnectedSocket(net::OK);
  flag->Cancel();
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_Success_ConcurrentCalls) {
  Initialize(WifiInitState::kComplete);

  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_socket_factory=*/kNumThreads,
      /*expected_success=*/true,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_socket_factory_->FinishNextCreateConnectedSocket(net::OK);
  }
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_Failure) {
  Initialize(WifiInitState::kComplete);

  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/1u,
      /*expected_success=*/false,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());
  fake_socket_factory_->FinishNextCreateConnectedSocket(net::ERR_FAILED);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_Failure_ConcurrentCalls) {
  Initialize(WifiInitState::kComplete);

  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_socket_factory=*/kNumThreads,
      /*expected_success=*/false,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_socket_factory_->FinishNextCreateConnectedSocket(net::ERR_FAILED);
  }
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_DestroyWhileWaiting) {
  Initialize(WifiInitState::kComplete);

  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_socket_factory=*/kNumThreads,
      /*expected_success=*/false,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());

  // The WifiLanMedium cancels all pending calls before destruction.
  wifi_lan_medium_.reset();
  run_loop.Run();
}
/*============================================================================*/
// End: ConnectToService()
/*============================================================================*/

/*============================================================================*/
// Begin: ListenForService()
/*============================================================================*/
TEST_F(WifiLanMediumTest, Listen_Success) {
  Initialize(WifiInitState::kComplete);

  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/1u,
      /*expected_success=*/true,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  fake_socket_factory_->FinishNextCreateServerSocket(net::OK);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Success_ConcurrentCalls) {
  Initialize(WifiInitState::kComplete);

  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_socket_factory=*/kNumThreads,
      /*expected_success=*/true,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_socket_factory_->FinishNextCreateServerSocket(net::OK);
  }
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Failure_InvalidPort) {
  Initialize(WifiInitState::kComplete);

  base::ScopedAllowBaseSyncPrimitivesForTesting allow;
  EXPECT_FALSE(wifi_lan_medium_->ListenForService(/*port=*/-1));
}

TEST_F(WifiLanMediumTest, Listen_Failure_FetchIp_GetNetworkStateList) {
  // Fail to fetch the local IP address because the network state list is empty.
  Initialize(WifiInitState::kNoWifiService);

  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/0u,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Failure_FetchIp_GetManagedProperties) {
  // Fail to retrieve the managed properties (including the IP address) of the
  // local Wi-Fi network.
  Initialize(WifiInitState::kNoManagedProperties);

  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/0u,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Failure_FetchIp_MissingIpConfigs) {
  // Successfully retrieve the managed properties of the local Wi-Fi network,
  // but they are missing the IP configs.
  Initialize(WifiInitState::kNoIpConfigs);

  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/0u,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Failure_FetchIp_IpAddressInvalid) {
  // Successfully retrieve the local IP address, but it is invalid.
  Initialize(WifiInitState::kIpAddressInvalid);

  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/0u,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Failure_CreateTcpServerSocket) {
  Initialize(WifiInitState::kComplete);

  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/1u,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  fake_socket_factory_->FinishNextCreateServerSocket(net::ERR_FAILED);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest,
       Listen_Failure_CreateTcpServerSocket_ConcurrentCalls) {
  Initialize(WifiInitState::kComplete);

  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_socket_factory=*/kNumThreads,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_socket_factory_->FinishNextCreateServerSocket(net::ERR_FAILED);
  }
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Failure_CreateFirewallHole) {
  Initialize(WifiInitState::kComplete);

  // Fail to create the firewall hole.
  fake_firewall_hole_factory_->should_succeed_ = false;

  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_socket_factory=*/1u,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  fake_socket_factory_->FinishNextCreateServerSocket(net::OK);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_DestroyWhileWaiting) {
  Initialize(WifiInitState::kComplete);

  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_socket_factory=*/kNumThreads,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());

  // The WifiLanMedium cancels all pending calls before destruction.
  wifi_lan_medium_.reset();
  run_loop.Run();
}
/*============================================================================*/
// End: ListenForService()
/*============================================================================*/

TEST_F(WifiLanMediumTest, GetDynamicPortRange) {
  Initialize(WifiInitState::kComplete);

  absl::optional<std::pair<std::int32_t, std::int32_t>> port_range =
      wifi_lan_medium_->GetDynamicPortRange();

  EXPECT_EQ(ash::nearby::TcpServerSocketPort::kMin, port_range->first);
  EXPECT_EQ(ash::nearby::TcpServerSocketPort::kMax, port_range->second);
}

}  // namespace chrome
}  // namespace nearby
