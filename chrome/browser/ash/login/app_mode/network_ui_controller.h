// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_NETWORK_UI_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_NETWORK_UI_CONTROLLER_H_

#include "base/auto_reset.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace {
class NetworkMonitor;
}

namespace ash {

class LoginDisplayHost;

class NetworkUiController
    : public AppLaunchSplashScreenView::Delegate,
      public KioskAppLauncher::NetworkDelegate,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  enum NetworkUIState {
    kNotShowing = 0,     // Network configure UI is not being shown.
    kNeedToShow,         // We need to show the UI as soon as we can.
    kShowing,            // Network configure UI is being shown.
    kWaitingForNetwork,  // App requested network and we're waiting for it
  };

  class Observer {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() = default;

    virtual void OnNetworkConfigureUiShowing() = 0;
    virtual void OnNetworkConfigureUiFinished() = 0;
    virtual void OnNetworkReady() = 0;
    virtual void OnNetworkLost() = 0;
  };

  class NetworkMonitor {
   public:
    using State = ash::NetworkStateInformer::State;
    using Observer = ash::NetworkStateInformer::NetworkStateInformerObserver;

    NetworkMonitor() = default;
    NetworkMonitor(const NetworkMonitor&) = delete;
    NetworkMonitor& operator=(const NetworkMonitor&) = delete;
    virtual ~NetworkMonitor() = default;

    virtual void AddObserver(Observer* observer) = 0;
    virtual void RemoveObserver(Observer* observer) = 0;
    virtual State GetState() const = 0;
    virtual std::string GetNetworkName() const = 0;
  };

  NetworkUiController(Observer& observer,
                      LoginDisplayHost* host,
                      AppLaunchSplashScreenView& splash_screen,
                      std::unique_ptr<NetworkMonitor> network_monitor);
  NetworkUiController(const NetworkUiController&) = delete;
  NetworkUiController& operator=(const NetworkUiController&) = delete;
  ~NetworkUiController() override;

  void Start();
  void SetProfile(Profile* profile);
  void UserRequestedNetworkConfig();
  bool ShouldShowNetworkConfig();
  void OnNetworkLostDuringInstallation();

  // `AppLaunchSplashScreenView::Delegate`
  void OnConfigureNetwork() override;
  void OnNetworkConfigFinished() override;

  // `KioskAppLauncher::NetworkDelegate`
  void InitializeNetwork() override;
  bool IsNetworkReady() const override;

  // `NetworkStateInformer::NetworkStateInformerObserver`
  void UpdateState(NetworkError::ErrorReason reason) override;

  NetworkUIState GetNetworkUiStateForTesting() const {
    return network_ui_state_;
  }

  static std::unique_ptr<base::AutoReset<absl::optional<bool>>>
  SetCanConfigureNetworkForTesting(bool can_configure_network);

 private:
  void OnNetworkStateChanged(bool online);
  void MaybeShowNetworkConfigureUI();
  void MaybeShowNetworkConfigureUIForConsumerKiosk();
  void ShowNetworkConfigureUI();
  void CloseNetworkConfigureUI();

  void OnNetworkWaitTimeout();
  bool CanConfigureNetwork();

  void OnNetworkOnline();
  void OnNetworkOffline();

  const raw_ref<Observer> observer_;
  const raw_ptr<LoginDisplayHost> host_;
  const raw_ref<AppLaunchSplashScreenView> splash_screen_view_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<NetworkMonitor> network_monitor_;

  NetworkUIState network_ui_state_ = kNotShowing;
  bool network_required_ = false;

  // A timer that fires when the network was not prepared and we require user
  // network configuration to continue.
  base::OneShotTimer network_wait_timer_;
  bool network_wait_timeout_ = false;

  base::ScopedObservation<NetworkMonitor,
                          NetworkStateInformer::NetworkStateInformerObserver>
      network_observation_{this};
  base::WeakPtrFactory<NetworkUiController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_NETWORK_UI_CONTROLLER_H_
