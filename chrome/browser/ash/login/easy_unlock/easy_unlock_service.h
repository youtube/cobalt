// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/easy_unlock/chrome_proximity_auth_client.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_auth_attempt.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_metrics.h"
#include "chrome/browser/ash/login/easy_unlock/smartlock_state_handler.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "components/keyed_service/core/keyed_service.h"

class AccountId;

namespace user_manager {
class User;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace proximity_auth {
class ProximityAuthPrefManager;
class ProximityAuthSystem;
}  // namespace proximity_auth

class Profile;

namespace ash {

enum class SmartLockState;

namespace secure_channel {
class SecureChannelClient;
}

class EasyUnlockService : public KeyedService,
                          public proximity_auth::ScreenlockBridge::Observer {
 public:
  // Gets EasyUnlockService instance.
  static EasyUnlockService* Get(Profile* profile);

  // Gets EasyUnlockService instance associated with a user if the user is
  // logged in and their profile is initialized.
  static EasyUnlockService* GetForUser(const user_manager::User& user);

  EasyUnlockService(const EasyUnlockService&) = delete;
  EasyUnlockService& operator=(const EasyUnlockService&) = delete;

  // Registers Easy Unlock profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the ProximityAuthPrefManager, responsible for managing all
  // EasyUnlock preferences.
  virtual proximity_auth::ProximityAuthPrefManager*
  GetProximityAuthPrefManager();

  // Returns the user currently associated with the service.
  virtual AccountId GetAccountId() const = 0;

  // Sets the service up and schedules service initialization.
  void Initialize();

  // Whether easy unlock is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled. Virtual to allow override for testing.
  virtual bool IsAllowed() const;

  // Whether Easy Unlock is currently enabled for this user. Virtual to allow
  // override for testing.
  virtual bool IsEnabled() const;

  // To be called when EasyUnlockService is "warming up", for example, on screen
  // lock, after suspend, when the login screen is starting up, etc. During a
  // period like this, not all sub-systems are fully initialized, particularly
  // UnlockManager and the Bluetooth stack, so to avoid UI jank, callers can use
  // this method to fill in the UI with an approximation of what the UI will
  // look like in <1 second. The resulting initial state will be one of two
  // possibilities:
  //   * SmartLockState::kConnectingToPhone: if the feature is allowed, enabled,
  //     and has kicked off a scan/connection.
  //   * SmartLockState::kDisabled: if any values above can't be confirmed.
  virtual SmartLockState GetInitialSmartLockState() const;

  // Updates the user pod on the signin/lock screen for the user associated with
  // the service to reflect the provided Smart Lock state.
  void UpdateSmartLockState(SmartLockState state);

  // Starts an auth attempt for the user associated with the service. The
  // attempt type (unlock vs. signin) will depend on the service type. Returns
  // true if no other attempt is in progress and the attempt request can be
  // processed.
  bool AttemptAuth(const AccountId& account_id);

  // Finalizes the previously started auth attempt for easy unlock. If called on
  // signin profile service, it will cancel the current auth attempt if one
  // exists.
  void FinalizeUnlock(bool success);

  // Handles Easy Unlock auth failure for the user.
  void HandleAuthFailure(const AccountId& account_id);

  ChromeProximityAuthClient* proximity_auth_client() {
    return &proximity_auth_client_;
  }

  // The last value emitted to the SmartLock.GetRemoteStatus.Unlock(.Failure)
  // metrics. Helps to understand whether/why not Smart Lock was an available
  // choice for unlock. Returns the empty string if the ProximityAuthSystem or
  // the UnlockManager is uninitialized.
  std::string GetLastRemoteStatusUnlockForLogging();

  // Retrieves the remote device list stored for the account in
  // |proximity_auth_system_|.
  const multidevice::RemoteDeviceRefList GetRemoteDevicesForTesting() const;

 protected:
  EasyUnlockService(Profile* profile,
                    secure_channel::SecureChannelClient* secure_channel_client);
  ~EasyUnlockService() override;

  // Does a service type specific initialization.
  virtual void InitializeInternal() = 0;

  // Does a service type specific shutdown. Called from `Shutdown`.
  virtual void ShutdownInternal() = 0;

  // Service type specific tests for whether the service is allowed. Returns
  // false if service is not allowed. If true is returned, the service may still
  // not be allowed if common tests fail (e.g. if Bluetooth is not available).
  virtual bool IsAllowedInternal() const = 0;

  // Called when the local device resumes after a suspend.
  virtual void OnSuspendDoneInternal() = 0;

  // Called when the user enters password before easy unlock succeeds or fails
  // definitively.
  virtual void OnUserEnteredPassword();

  // KeyedService:
  void Shutdown() override;

  // proximity_auth::ScreenlockBridge::Observer:
  void OnScreenDidLock(proximity_auth::ScreenlockBridge::LockHandler::ScreenType
                           screen_type) override;
  void OnScreenDidUnlock(
      proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type)
      override = 0;
  void OnFocusedUserChanged(const AccountId& account_id) override = 0;

  // Exposes the profile to which the service is attached to subclasses.
  Profile* profile() const { return profile_; }

  // Checks whether Easy unlock should be running and updates app state.
  void UpdateAppState();

  // Fill in the UI with the state returned by GetInitialSmartLockState().
  void ShowInitialSmartLockState();

  // Resets the Smart Lock state set by this service.
  void ResetSmartLockState();

  const SmartLockStateHandler* smartlock_state_handler() const {
    return smartlock_state_handler_.get();
  }

  // Returns the authentication event for a recent password sign-in or unlock,
  // according to the current state of the service.
  EasyUnlockAuthEvent GetPasswordAuthEvent() const;

  // Returns the authentication event for a recent password sign-in or unlock,
  // according to the current state of the service.
  SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
  GetSmartUnlockPasswordAuthEvent() const;

  // Called by subclasses when remote devices allowed to unlock the screen
  // are loaded for `account_id`.
  void SetProximityAuthDevices(
      const AccountId& account_id,
      const multidevice::RemoteDeviceRefList& remote_devices,
      absl::optional<multidevice::RemoteDeviceRef> local_device);

  bool will_authenticate_using_easy_unlock() const {
    return will_authenticate_using_easy_unlock_;
  }

  void set_will_authenticate_using_easy_unlock(
      bool will_authenticate_using_easy_unlock) {
    will_authenticate_using_easy_unlock_ = will_authenticate_using_easy_unlock;
  }

 private:
  // True if the user just authenticated using Easy Unlock. Reset once
  // the screen signs in/unlocks. Used to distinguish Easy Unlock-powered
  // signins/unlocks from password-based unlocks for metrics.
  bool will_authenticate_using_easy_unlock_ = false;

  // Gets `smartlock_state_handler_`. Returns NULL if Easy Unlock is not
  // allowed. Otherwise, if `smartlock_state_handler_` is not set, an instance
  // is created. Do not cache the returned value, as it may go away if Easy
  // Unlock gets disabled.
  SmartLockStateHandler* GetSmartLockStateHandler();

  // Updates the service to state for handling system suspend.
  void PrepareForSuspend();

  // Called when the system resumes from a suspended state.
  void OnSuspendDone();

  // Determines whether failure to unlock with phone should be handled as an
  // authentication failure.
  bool IsSmartLockStateValidOnRemoteAuthFailure() const;

  void NotifySmartLockAuthResult(bool success);

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  raw_ptr<secure_channel::SecureChannelClient, ExperimentalAsh>
      secure_channel_client_;

  ChromeProximityAuthClient proximity_auth_client_;

  // Created lazily in `GetSmartLockStateHandler`.
  std::unique_ptr<SmartLockStateHandler> smartlock_state_handler_;

  absl::optional<SmartLockState> smart_lock_state_;

  // The handler for the current auth attempt. Set iff an auth attempt is in
  // progress.
  std::unique_ptr<EasyUnlockAuthAttempt> auth_attempt_;

  // Handles connecting, authenticating, and updating the UI on the lock/sign-in
  // screen. After a `RemoteDeviceRef` instance is provided, this object will
  // handle the rest.
  std::unique_ptr<proximity_auth::ProximityAuthSystem> proximity_auth_system_;

  // Monitors suspend and wake state of ChromeOS.
  class PowerMonitor;
  std::unique_ptr<PowerMonitor> power_monitor_;

  // Whether the service has been shut down.
  bool shut_down_;

  base::WeakPtrFactory<EasyUnlockService> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_
