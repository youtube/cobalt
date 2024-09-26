// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/startup_app_launcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/chrome_app_kiosk_service_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "components/crx_file/id_util.h"

namespace ash {

namespace {

const int kMaxLaunchAttempt = 5;

// Reduced backoff policy for extension downloader while Kiosk is launching.
const net::BackoffEntry::Policy kKioskLaunchExtensionBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = 2000,
    .multiply_factor = 2,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = 3000,
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false,
};

crosapi::BrowserManager* browser_manager() {
  return crosapi::BrowserManager::Get();
}

crosapi::ChromeAppKioskServiceAsh* crosapi_chrome_app_kiosk_service() {
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->chrome_app_kiosk_service();
}

}  // namespace

class LacrosLauncher : public crosapi::BrowserManagerObserver {
 public:
  LacrosLauncher() = default;
  LacrosLauncher(const LacrosLauncher&) = delete;
  LacrosLauncher& operator=(const LacrosLauncher&) = delete;
  ~LacrosLauncher() override = default;

  void Start(base::OnceClosure callback) {
    if (browser_manager()->IsRunning()) {
      // Nothing to do if lacros is already running
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback));
      return;
    }

    callback_ = std::move(callback);
    browser_manager_observation_.Observe(browser_manager());
    if (!browser_manager()->IsInitialized()) {
      browser_manager()->InitializeAndStartIfNeeded();
    }
  }

 private:
  // crosapi::BrowserManagerObserver
  void OnStateChanged() override {
    if (crosapi::BrowserManager::Get()->IsRunning()) {
      browser_manager_observation_.Reset();
      std::move(callback_).Run();
    }
  }

  base::OnceClosure callback_;

  // Observe the launch state of `BrowserManager`, and launch the
  // lacros-chrome when it is ready. This object is only used when Lacros is
  // enabled.
  base::ScopedObservation<crosapi::BrowserManager,
                          crosapi::BrowserManagerObserver>
      browser_manager_observation_{this};
};

StartupAppLauncher::StartupAppLauncher(
    Profile* profile,
    const std::string& app_id,
    bool should_skip_install,
    StartupAppLauncher::NetworkDelegate* network_delegate)
    : KioskAppLauncher(network_delegate),
      profile_(profile),
      app_id_(app_id),
      should_skip_install_(should_skip_install) {
  DCHECK(profile_);
  DCHECK(crx_file::id_util::IdIsValid(app_id_));

  // Reduce extension downloader retry backoff to avoid waiting on splash screen
  // for a long time.
  KioskAppManager::Get()->SetExtensionDownloaderBackoffPolicy(
      kKioskLaunchExtensionBackoffPolicy);
}

StartupAppLauncher::~StartupAppLauncher() {
  // Restore to default extension downloader backoff policy.
  KioskAppManager::Get()->SetExtensionDownloaderBackoffPolicy(absl::nullopt);
}

void StartupAppLauncher::AddObserver(KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void StartupAppLauncher::RemoveObserver(KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void StartupAppLauncher::Initialize() {
  DCHECK(state_ != LaunchState::kReadyToLaunch &&
         state_ != LaunchState::kWaitingForWindow &&
         state_ != LaunchState::kLaunchSucceeded);

  if (should_skip_install_) {
    if (crosapi::browser_util::IsLacrosEnabledInChromeKioskSession()) {
      LaunchLacros(base::BindOnce(&StartupAppLauncher::OnInstallSuccess,
                                  weak_ptr_factory_.GetWeakPtr()));
    } else {
      OnInstallSuccess();
    }
    return;
  }

  // Wait until user has configured the network. We will come back into this
  // class through ContinueWithNetworkReady.
  if (delegate_->IsShowingNetworkConfigScreen()) {
    state_ = LaunchState::kInitializingNetwork;
    return;
  }

  // Update the offline enabled crx cache if the network is ready;
  // or just install the app.
  if (delegate_->IsNetworkReady()) {
    ContinueWithNetworkReady();
  } else {
    BeginInstall();
  }
}

void StartupAppLauncher::ContinueWithNetworkReady() {
  SYSLOG(INFO) << "ContinueWithNetworkReady"
               << ", state_="
               << static_cast<typename std::underlying_type<LaunchState>::type>(
                      state_);

  if (state_ != LaunchState::kInitializingNetwork &&
      state_ != LaunchState::kNotStarted) {
    return;
  }

  if (should_skip_install_) {
    OnInstallSuccess();
    return;
  }

  // The network might not be ready when KioskAppManager tries to update
  // external cache initially. Update the external cache now that the network
  // is ready for sure.
  state_ = LaunchState::kWaitingForCache;
  kiosk_app_manager_observation_.Observe(KioskAppManager::Get());
  KioskAppManager::Get()->UpdateExternalCache();
}

bool StartupAppLauncher::RetryWhenNetworkIsAvailable() {
  ++launch_attempt_;
  if (launch_attempt_ < kMaxLaunchAttempt) {
    state_ = LaunchState::kInitializingNetwork;
    delegate_->InitializeNetwork();
    return true;
  }
  return false;
}

void StartupAppLauncher::OnKioskExtensionLoadedInCache(
    const std::string& app_id) {
  OnKioskAppDataLoadStatusChanged(app_id);
}

void StartupAppLauncher::OnKioskExtensionDownloadFailed(
    const std::string& app_id) {
  OnKioskAppDataLoadStatusChanged(app_id);
}

void StartupAppLauncher::OnKioskAppDataLoadStatusChanged(
    const std::string& app_id) {
  DCHECK(state_ == LaunchState::kWaitingForCache);

  if (app_id != app_id_) {
    return;
  }

  kiosk_app_manager_observation_.Reset();

  if (KioskAppManager::Get()->HasCachedCrx(app_id_)) {
    BeginInstall();
  } else {
    OnLaunchFailure(KioskAppLaunchError::Error::kUnableToDownload);
  }
}

void StartupAppLauncher::BeginInstall() {
  if (crosapi::browser_util::IsLacrosEnabledInChromeKioskSession()) {
    // We need to make sure that the Lacros browser is running before we can
    // install the kiosk app.
    LaunchLacros(base::BindOnce(&StartupAppLauncher::InstallAppInLacros,
                                weak_ptr_factory_.GetWeakPtr()));
  } else {
    InstallAppInAsh();
  }
}

void StartupAppLauncher::LaunchLacros(base::OnceClosure next_step) {
  state_ = LaunchState::kWaitingForLacros;
  lacros_launcher_ = std::make_unique<LacrosLauncher>();
  lacros_launcher_->Start(std::move(next_step));
}

void StartupAppLauncher::InstallAppInAsh() {
  state_ = LaunchState::kInstallingApp;
  observers_.NotifyAppInstalling();
  installer_ = std::make_unique<ChromeKioskAppInstaller>(
      profile_, KioskAppManager::Get()->CreatePrimaryAppInstallData(app_id_));
  installer_->BeginInstall(base::BindOnce(
      &StartupAppLauncher::OnInstallComplete, weak_ptr_factory_.GetWeakPtr()));
}

void StartupAppLauncher::InstallAppInLacros() {
  DCHECK(state_ == LaunchState::kWaitingForLacros);
  state_ = LaunchState::kInstallingApp;
  observers_.NotifyAppInstalling();
  crosapi_chrome_app_kiosk_service()->InstallKioskApp(
      KioskAppManager::Get()->CreatePrimaryAppInstallData(app_id_),
      base::BindOnce(&StartupAppLauncher::OnInstallComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StartupAppLauncher::OnInstallComplete(
    ChromeKioskAppInstaller::InstallResult result) {
  DCHECK(state_ == LaunchState::kInstallingApp);

  installer_.reset();

  switch (result) {
    case ChromeKioskAppInstaller::InstallResult::kSuccess:
      OnInstallSuccess();
      return;
    case ChromeKioskAppInstaller::InstallResult::kPrimaryAppInstallFailed:
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToInstall);
      return;
    case ChromeKioskAppInstaller::InstallResult::kPrimaryAppNotKioskEnabled:
      OnLaunchFailure(KioskAppLaunchError::Error::kNotKioskEnabled);
      return;
    case ChromeKioskAppInstaller::InstallResult::kPrimaryAppNotCached:
    case ChromeKioskAppInstaller::InstallResult::kSecondaryAppInstallFailed:
      if (!RetryWhenNetworkIsAvailable()) {
        OnLaunchFailure(KioskAppLaunchError::Error::kUnableToInstall);
      }
      return;
    case ChromeKioskAppInstaller::InstallResult::kUnknown:
      SYSLOG(ERROR) << "Received unknown InstallResult";
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToInstall);
      return;
  }
}

void StartupAppLauncher::OnInstallSuccess() {
  state_ = LaunchState::kReadyToLaunch;

  observers_.NotifyAppPrepared();
}

void StartupAppLauncher::LaunchApp() {
  if (state_ != LaunchState::kReadyToLaunch) {
    NOTREACHED();
    SYSLOG(ERROR) << "LaunchApp() called but launcher is not initialized.";
  }

  if (crosapi::browser_util::IsLacrosEnabledInChromeKioskSession()) {
    crosapi_chrome_app_kiosk_service()->LaunchKioskApp(
        app_id_, delegate_->IsNetworkReady(),
        base::BindOnce(&StartupAppLauncher::OnLaunchComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    launcher_ = std::make_unique<ChromeKioskAppLauncher>(
        profile_, app_id_, delegate_->IsNetworkReady());

    launcher_->LaunchApp(base::BindOnce(&StartupAppLauncher::OnLaunchComplete,
                                        weak_ptr_factory_.GetWeakPtr()));
  }
}

void StartupAppLauncher::OnLaunchComplete(
    ChromeKioskAppLauncher::LaunchResult result) {
  DCHECK(state_ == LaunchState::kReadyToLaunch);

  launcher_.reset();

  switch (result) {
    case ChromeKioskAppLauncher::LaunchResult::kSuccess:
      OnLaunchSuccess();
      return;
    case ChromeKioskAppLauncher::LaunchResult::kUnableToLaunch:
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      return;
    case ChromeKioskAppLauncher::LaunchResult::kNetworkMissing:
      if (!RetryWhenNetworkIsAvailable()) {
        OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      }
      return;
    case ChromeKioskAppLauncher::LaunchResult::kUnknown:
      SYSLOG(ERROR) << "Received unknown LaunchResult";
      OnLaunchFailure(KioskAppLaunchError::Error::kUnableToLaunch);
      return;
  }
}

void StartupAppLauncher::OnLaunchSuccess() {
  state_ = LaunchState::kLaunchSucceeded;
  observers_.NotifyAppLaunched();
  observers_.NotifyAppWindowCreated();
}

void StartupAppLauncher::OnLaunchFailure(KioskAppLaunchError::Error error) {
  SYSLOG(ERROR) << "App launch failed, error: " << static_cast<int>(error);
  DCHECK_NE(KioskAppLaunchError::Error::kNone, error);

  observers_.NotifyLaunchFailed(error);
}

}  // namespace ash
