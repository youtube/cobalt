// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "base/syslog_logging.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/network_service_instance.h"

namespace {

// Time of waiting for the network to be ready to start installation. Can be
// changed in tests.
constexpr base::TimeDelta kKioskNetworkWaitTime = base::Seconds(10);
base::TimeDelta g_network_wait_time = kKioskNetworkWaitTime;

absl::optional<bool> g_can_configure_network_for_testing;

bool IsDeviceEnterpriseManaged() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->IsDeviceEnterpriseManaged();
}

bool IsConsumerKiosk() {
  return !IsDeviceEnterpriseManaged();
}

bool CanConfigureNetworkForConsumerKiosk() {
  return user_manager::UserManager::Get()->GetOwnerAccountId().is_valid();
}

bool CanConfigureNetworkForEnterpriseKiosk() {
  bool should_prompt;
  if (ash::CrosSettings::Get()->GetBoolean(
          ash::kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline,
          &should_prompt)) {
    return should_prompt;
  }
  // Default to true to allow network configuration if the policy is
  // missing.
  return true;
}

network::mojom::ConnectionType GetCurrentConnectionType() {
  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  content::GetNetworkConnectionTracker()->GetConnectionType(&connection_type,
                                                            base::DoNothing());
  return connection_type;
}

}  // namespace

namespace ash {

NetworkUiController::NetworkUiController(
    Observer& observer,
    LoginDisplayHost* host,
    AppLaunchSplashScreenView& splash_screen,
    std::unique_ptr<NetworkMonitor> network_monitor)
    : observer_(observer),
      host_(host),
      splash_screen_view_(splash_screen),
      network_monitor_(std::move(network_monitor)) {
  if (!host_) {
    CHECK_IS_TEST();
  }
  splash_screen_view_->SetDelegate(this);
}

NetworkUiController::~NetworkUiController() {
  splash_screen_view_->SetDelegate(nullptr);
}

void NetworkUiController::Start() {
  network_observation_.Observe(network_monitor_.get());
}

void NetworkUiController::SetProfile(Profile* profile) {
  profile_ = profile;
}

void NetworkUiController::UserRequestedNetworkConfig() {
  if (!profile_) {
    SYSLOG(INFO) << "Postponing network dialog till profile is loaded.";
    network_ui_state_ = NetworkUIState::kNeedToShow;
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kShowingNetworkConfigureUI);
    return;
  }

  MaybeShowNetworkConfigureUI();
}

bool NetworkUiController::ShouldShowNetworkConfig() {
  return network_ui_state_ == kNeedToShow;
}

void NetworkUiController::OnNetworkLostDuringInstallation() {
  if (network_required_) {
    SYSLOG(WARNING) << "Connection lost during installation.";
    OnNetworkWaitTimeout();
  }
}

void NetworkUiController::InitializeNetwork() {
  network_ui_state_ = NetworkUIState::kWaitingForNetwork;

  network_wait_timer_.Start(FROM_HERE, g_network_wait_time, this,
                            &NetworkUiController::OnNetworkWaitTimeout);

  // Asking to initialize network means the app requires network. Remember that.
  network_required_ = true;

  splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::kPreparingNetwork);

  if (IsNetworkReady()) {
    OnNetworkOnline();
  }
}

void NetworkUiController::OnConfigureNetwork() {
  CHECK(IsConsumerKiosk());
  CHECK(profile_);

  if (network_ui_state_ == NetworkUIState::kShowing) {
    return;
  }

  if (CanConfigureNetworkForConsumerKiosk() && host_) {
    host_->VerifyOwnerForKiosk(
        base::BindOnce(&NetworkUiController::ShowNetworkConfigureUI,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (!host_) {
    CHECK_IS_TEST();
  }
}

void NetworkUiController::OnNetworkConfigFinished() {
  network_ui_state_ = NetworkUIState::kNotShowing;

  observer_->OnNetworkConfigureUiFinished();
}

void NetworkUiController::UpdateState(NetworkError::ErrorReason) {
  OnNetworkStateChanged(IsNetworkReady());
}

void NetworkUiController::OnNetworkStateChanged(bool online) {
  if (online) {
    OnNetworkOnline();
  } else {
    OnNetworkOffline();
  }

  // If the network configure UI is currently showing, redraw it to reflect the
  // changed network state.
  if (network_ui_state_ == kShowing) {
    ShowNetworkConfigureUI();
  }
}

void NetworkUiController::OnNetworkOnline() {
  bool network_showing_after_timeout =
      network_wait_timeout_ && network_ui_state_ == kShowing;
  bool is_waiting_for_network = network_ui_state_ == kWaitingForNetwork;

  if (!is_waiting_for_network && !network_showing_after_timeout) {
    // The UI is not showing at all, or was requested by the user so we do
    // nothing
    return;
  }

  network_wait_timer_.Stop();
  network_ui_state_ = kNotShowing;

  if (network_showing_after_timeout) {
    SYSLOG(INFO) << "We are back online, closing network configure screen.";
    CloseNetworkConfigureUI();
  } else {
    observer_->OnNetworkReady();
  }
}

void NetworkUiController::OnNetworkOffline() {
  observer_->OnNetworkLost();
}

void NetworkUiController::CloseNetworkConfigureUI() {
  splash_screen_view_->ToggleNetworkConfig(false);
  splash_screen_view_->ContinueAppLaunch();
}

bool NetworkUiController::IsNetworkReady() const {
  return network_monitor_->GetState() == NetworkStateInformer::ONLINE;
}

void NetworkUiController::MaybeShowNetworkConfigureUI() {
  SYSLOG(INFO) << "Network configure UI was requested to be shown.";
  if (!CanConfigureNetwork()) {
    splash_screen_view_->UpdateAppLaunchState(
        AppLaunchSplashScreenView::AppLaunchState::kNetworkWaitTimeout);
    return;
  }

  if (IsConsumerKiosk()) {
    MaybeShowNetworkConfigureUIForConsumerKiosk();
  } else {
    ShowNetworkConfigureUI();
  }
}

void NetworkUiController::ShowNetworkConfigureUI() {
  // We should stop timers since they may fire during network
  // configure UI.
  network_wait_timer_.Stop();
  network_ui_state_ = NetworkUIState::kShowing;
  NetworkStateInformer::State state = network_monitor_->GetState();
  // We should not block users when the network was not required by the
  // controller.
  if (!network_required_) {
    state = NetworkStateInformer::ONLINE;
  }
  splash_screen_view_->ShowNetworkConfigureUI(
      state, network_monitor_->GetNetworkName());

  observer_->OnNetworkConfigureUiShowing();
}

void NetworkUiController::OnNetworkWaitTimeout() {
  DCHECK(network_ui_state_ == NetworkUIState::kNotShowing ||
         network_ui_state_ == NetworkUIState::kWaitingForNetwork);

  network::mojom::ConnectionType connection_type = GetCurrentConnectionType();
  SYSLOG(WARNING) << __FUNCTION__ << " connection = " << connection_type;
  network_wait_timeout_ = true;

  MaybeShowNetworkConfigureUI();
}

bool NetworkUiController::CanConfigureNetwork() {
  if (g_can_configure_network_for_testing.has_value()) {
    return g_can_configure_network_for_testing.value();
  }

  if (IsDeviceEnterpriseManaged()) {
    return CanConfigureNetworkForEnterpriseKiosk();
  }

  return CanConfigureNetworkForConsumerKiosk();
}

void NetworkUiController::MaybeShowNetworkConfigureUIForConsumerKiosk() {
  if (!network_wait_timeout_) {
    OnConfigureNetwork();
  } else {
    splash_screen_view_->ToggleNetworkConfig(true);
  }
}

// static
std::unique_ptr<base::AutoReset<absl::optional<bool>>>
NetworkUiController::SetCanConfigureNetworkForTesting(
    bool can_configure_network) {
  return std::make_unique<base::AutoReset<absl::optional<bool>>>(
      &g_can_configure_network_for_testing, can_configure_network);
}

}  // namespace ash
