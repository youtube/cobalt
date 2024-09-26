// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/smartlock_state.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/easy_unlock/chrome_proximity_auth_client.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_system.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

void SetAuthTypeIfChanged(
    proximity_auth::ScreenlockBridge::LockHandler* lock_handler,
    const AccountId& account_id,
    proximity_auth::mojom::AuthType auth_type,
    const std::u16string& auth_value) {
  DCHECK(lock_handler);
  const proximity_auth::mojom::AuthType existing_auth_type =
      lock_handler->GetAuthType(account_id);
  if (auth_type == existing_auth_type)
    return;

  lock_handler->SetAuthType(account_id, auth_type, auth_value);
}

}  // namespace

// static
EasyUnlockService* EasyUnlockService::Get(Profile* profile) {
  return EasyUnlockServiceFactory::GetForBrowserContext(profile);
}

// static
EasyUnlockService* EasyUnlockService::GetForUser(
    const user_manager::User& user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(&user);
  if (!profile)
    return nullptr;
  return EasyUnlockService::Get(profile);
}

class EasyUnlockService::PowerMonitor
    : public chromeos::PowerManagerClient::Observer {
 public:
  explicit PowerMonitor(EasyUnlockService* service) : service_(service) {
    chromeos::PowerManagerClient::Get()->AddObserver(this);
  }

  PowerMonitor(const PowerMonitor&) = delete;
  PowerMonitor& operator=(const PowerMonitor&) = delete;

  ~PowerMonitor() override {
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  }

 private:
  // PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override {
    service_->PrepareForSuspend();
  }

  void SuspendDone(base::TimeDelta sleep_duration) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerMonitor::ResetWakingUp,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(5));
    service_->OnSuspendDone();
    service_->UpdateAppState();
    // Note that `this` may get deleted after `UpdateAppState` is called.
  }

  void ResetWakingUp() { service_->UpdateAppState(); }

  raw_ptr<EasyUnlockService, ExperimentalAsh> service_;
  base::WeakPtrFactory<PowerMonitor> weak_ptr_factory_{this};
};

EasyUnlockService::EasyUnlockService(
    Profile* profile,
    secure_channel::SecureChannelClient* secure_channel_client)
    : profile_(profile),
      secure_channel_client_(secure_channel_client),
      proximity_auth_client_(profile),
      shut_down_(false) {}

EasyUnlockService::~EasyUnlockService() = default;

// static
void EasyUnlockService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kEasyUnlockPairing);
  proximity_auth::ProximityAuthProfilePrefManager::RegisterPrefs(registry);
}

void EasyUnlockService::Initialize() {
  proximity_auth::ScreenlockBridge::Get()->AddObserver(this);

  InitializeInternal();
}

proximity_auth::ProximityAuthPrefManager*
EasyUnlockService::GetProximityAuthPrefManager() {
  NOTREACHED();
  return nullptr;
}

bool EasyUnlockService::IsAllowed() const {
  if (shut_down_)
    return false;

  if (!IsAllowedInternal())
    return false;

  return true;
}

bool EasyUnlockService::IsEnabled() const {
  return false;
}

SmartLockState EasyUnlockService::GetInitialSmartLockState() const {
  if (IsAllowed() && IsEnabled() && proximity_auth_system_ != nullptr)
    return SmartLockState::kConnectingToPhone;

  return SmartLockState::kDisabled;
}

SmartLockStateHandler* EasyUnlockService::GetSmartLockStateHandler() {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
    return nullptr;

  if (!IsAllowed())
    return nullptr;
  if (!smartlock_state_handler_) {
    smartlock_state_handler_ = std::make_unique<SmartLockStateHandler>(
        GetAccountId(), proximity_auth::ScreenlockBridge::Get());
  }
  return smartlock_state_handler_.get();
}

void EasyUnlockService::UpdateSmartLockState(SmartLockState state) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    if (smart_lock_state_ && state == smart_lock_state_.value())
      return;

    smart_lock_state_ = state;

    if (proximity_auth::ScreenlockBridge::Get()->IsLocked()) {
      auto* lock_handler =
          proximity_auth::ScreenlockBridge::Get()->lock_handler();
      DCHECK(lock_handler);

      lock_handler->SetSmartLockState(GetAccountId(), state);

      // TODO(https://crbug.com/1233614): Eventually we would like to remove
      // auth_type.mojom where AuthType lives, but this will require further
      // investigation. This logic was copied from
      // SmartLockStateHandler::UpdateScreenlockAuthType.
      // Do not override online signin.
      if (lock_handler->GetAuthType(GetAccountId()) !=
          proximity_auth::mojom::AuthType::ONLINE_SIGN_IN) {
        if (smart_lock_state_ == SmartLockState::kPhoneAuthenticated) {
          SetAuthTypeIfChanged(
              lock_handler, GetAccountId(),
              proximity_auth::mojom::AuthType::USER_CLICK,
              l10n_util::GetStringUTF16(
                  IDS_EASY_UNLOCK_SCREENLOCK_USER_POD_AUTH_VALUE));
        } else {
          SetAuthTypeIfChanged(
              lock_handler, GetAccountId(),
              proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
              std::u16string());
        }
      }
    }

    if (state != SmartLockState::kPhoneAuthenticated && auth_attempt_) {
      // Clean up existing auth attempt if we can no longer authenticate the
      // remote device.
      auth_attempt_.reset();

      if (!IsSmartLockStateValidOnRemoteAuthFailure())
        HandleAuthFailure(GetAccountId());
    }

    return;
  }

  SmartLockStateHandler* handler = GetSmartLockStateHandler();
  if (!handler)
    return;

  handler->ChangeState(state);

  if (state != SmartLockState::kPhoneAuthenticated && auth_attempt_) {
    auth_attempt_.reset();

    if (!handler->InStateValidOnRemoteAuthFailure())
      HandleAuthFailure(GetAccountId());
  }
}

void EasyUnlockService::OnUserEnteredPassword() {
  if (proximity_auth_system_)
    proximity_auth_system_->CancelConnectionAttempt();
}

bool EasyUnlockService::AttemptAuth(const AccountId& account_id) {
  PA_LOG(VERBOSE) << "User began unlock auth attempt.";

  if (auth_attempt_) {
    PA_LOG(VERBOSE) << "Already attempting auth, skipping this request.";
    return false;
  }

  if (!GetAccountId().is_valid()) {
    PA_LOG(ERROR) << "Empty user account. Auth attempt failed.";
    SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kEmptyUserAccount);
    return false;
  }

  if (GetAccountId() != account_id) {
    SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kInvalidAccoundId);

    PA_LOG(ERROR) << "Check failed: " << GetAccountId().Serialize() << " vs "
                  << account_id.Serialize();
    return false;
  }

  auth_attempt_ = std::make_unique<EasyUnlockAuthAttempt>(account_id);
  if (!auth_attempt_->Start()) {
    SmartLockMetricsRecorder::RecordAuthResultUnlockFailure(
        SmartLockMetricsRecorder::SmartLockAuthResultFailureReason::
            kAuthAttemptCannotStart);
    auth_attempt_.reset();
    return false;
  }

  // TODO(tengs): We notify ProximityAuthSystem whenever unlock attempts are
  // attempted. However, we ideally should refactor the auth attempt logic to
  // the proximity_auth component.
  if (!proximity_auth_system_) {
    PA_LOG(ERROR) << "No ProximityAuthSystem present.";
    return false;
  }

  proximity_auth_system_->OnAuthAttempted();
  return true;
}

void EasyUnlockService::FinalizeUnlock(bool success) {
  if (!auth_attempt_)
    return;

  set_will_authenticate_using_easy_unlock(true);
  auth_attempt_->FinalizeUnlock(GetAccountId(), success);

  // If successful, allow |auth_attempt_| to continue until
  // UpdateSmartLockState() is called (indicating screen unlock).

  // Make sure that the lock screen is updated on failure.
  if (!success) {
    auth_attempt_.reset();
    RecordEasyUnlockScreenUnlockEvent(EASY_UNLOCK_FAILURE);
    if (!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
      HandleAuthFailure(GetAccountId());
    }
  }

  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    NotifySmartLockAuthResult(success);
  }
}

void EasyUnlockService::HandleAuthFailure(const AccountId& account_id) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    NotifySmartLockAuthResult(/*success=*/false);
    return;
  }

  if (account_id != GetAccountId())
    return;

  if (!smartlock_state_handler_.get())
    return;
}

void EasyUnlockService::Shutdown() {
  if (shut_down_)
    return;
  shut_down_ = true;

  ShutdownInternal();

  proximity_auth::ScreenlockBridge::Get()->RemoveObserver(this);

  ResetSmartLockState();
  proximity_auth_system_.reset();
  power_monitor_.reset();

  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EasyUnlockService::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    ShowInitialSmartLockState();
  }
}

void EasyUnlockService::UpdateAppState() {
  if (IsAllowed()) {
    if (proximity_auth_system_)
      proximity_auth_system_->Start();

    if (!power_monitor_)
      power_monitor_ = std::make_unique<PowerMonitor>(this);
  }
}

void EasyUnlockService::ShowInitialSmartLockState() {
  // Only proceed if the screen is locked to prevent the UI event from not
  // persisting within UpdateSmartLockState().
  //
  // Note: ScreenlockBridge::IsLocked() may return a false positive if the
  // system is "warming up" (for example, ScreenlockBridge::IsLocked() will
  // return false when EasyUnlockServiceSignin is first instantiated because of
  // initialization timing in UserSelectionScreen). To work around this race,
  // ShowInitialSmartLockState() is also called from OnScreenDidLock() (which
  // triggers when ScreenlockBridge::IsLocked() becomes true) to ensure that
  // an initial state is displayed in the UI.
  auto* screenlock_bridge = proximity_auth::ScreenlockBridge::Get();
  if (screenlock_bridge && screenlock_bridge->IsLocked()) {
    UpdateSmartLockState(GetInitialSmartLockState());
  }
}

void EasyUnlockService::ResetSmartLockState() {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    smart_lock_state_.reset();
  } else {
    smartlock_state_handler_.reset();
  }

  auth_attempt_.reset();
}

SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
EasyUnlockService::GetSmartUnlockPasswordAuthEvent() const {
  DCHECK(IsEnabled());

  if (!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp) &&
      !smartlock_state_handler()) {
    return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
        kUnknownState;
  } else if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp) &&
             !smart_lock_state_) {
    return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
        kUnknownState;
  } else {
    SmartLockState state =
        (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
            ? smart_lock_state_.value()
            : smartlock_state_handler()->state();
    switch (state) {
      case SmartLockState::kInactive:
      case SmartLockState::kDisabled:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kServiceNotActive;
      case SmartLockState::kBluetoothDisabled:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kNoBluetooth;
      case SmartLockState::kConnectingToPhone:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kBluetoothConnecting;
      case SmartLockState::kPhoneNotFound:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kCouldNotConnectToPhone;
      case SmartLockState::kPhoneNotAuthenticated:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kNotAuthenticated;
      case SmartLockState::kPhoneFoundLockedAndProximate:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneLocked;
      case SmartLockState::kPhoneFoundUnlockedAndDistant:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kRssiTooLow;
      case SmartLockState::kPhoneFoundLockedAndDistant:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneLockedAndRssiTooLow;
      case SmartLockState::kPhoneAuthenticated:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kAuthenticatedPhone;
      case SmartLockState::kPhoneNotLockable:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPhoneNotLockable;
      case SmartLockState::kPrimaryUserAbsent:
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kPrimaryUserAbsent;
      default:
        NOTREACHED();
        return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
            kUnknownState;
    }
  }

  NOTREACHED();
  return SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
      kUnknownState;
}

EasyUnlockAuthEvent EasyUnlockService::GetPasswordAuthEvent() const {
  DCHECK(IsEnabled());

  if (!base::FeatureList::IsEnabled(features::kSmartLockUIRevamp) &&
      !smartlock_state_handler()) {
    return PASSWORD_ENTRY_NO_SMARTLOCK_STATE_HANDLER;
  } else if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp) &&
             !smart_lock_state_) {
    return PASSWORD_ENTRY_NO_SMARTLOCK_STATE_HANDLER;
  } else {
    SmartLockState state =
        (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp))
            ? smart_lock_state_.value()
            : smartlock_state_handler()->state();

    switch (state) {
      case SmartLockState::kInactive:
      case SmartLockState::kDisabled:
        return PASSWORD_ENTRY_SERVICE_NOT_ACTIVE;
      case SmartLockState::kBluetoothDisabled:
        return PASSWORD_ENTRY_NO_BLUETOOTH;
      case SmartLockState::kConnectingToPhone:
        return PASSWORD_ENTRY_BLUETOOTH_CONNECTING;
      case SmartLockState::kPhoneNotFound:
        return PASSWORD_ENTRY_NO_PHONE;
      case SmartLockState::kPhoneNotAuthenticated:
        return PASSWORD_ENTRY_PHONE_NOT_AUTHENTICATED;
      case SmartLockState::kPhoneFoundLockedAndProximate:
        return PASSWORD_ENTRY_PHONE_LOCKED;
      case SmartLockState::kPhoneNotLockable:
        return PASSWORD_ENTRY_PHONE_NOT_LOCKABLE;
      case SmartLockState::kPhoneFoundUnlockedAndDistant:
        return PASSWORD_ENTRY_RSSI_TOO_LOW;
      case SmartLockState::kPhoneFoundLockedAndDistant:
        return PASSWORD_ENTRY_PHONE_LOCKED_AND_RSSI_TOO_LOW;
      case SmartLockState::kPhoneAuthenticated:
        return PASSWORD_ENTRY_WITH_AUTHENTICATED_PHONE;
      case SmartLockState::kPrimaryUserAbsent:
        return PASSWORD_ENTRY_PRIMARY_USER_ABSENT;
    }
  }

  NOTREACHED();
  return EASY_UNLOCK_AUTH_EVENT_COUNT;
}

void EasyUnlockService::SetProximityAuthDevices(
    const AccountId& account_id,
    const multidevice::RemoteDeviceRefList& remote_devices,
    absl::optional<multidevice::RemoteDeviceRef> local_device) {
  UMA_HISTOGRAM_COUNTS_100("SmartLock.EnabledDevicesCount",
                           remote_devices.size());

  if (remote_devices.size() == 0) {
    proximity_auth_system_.reset();
    return;
  }

  if (!proximity_auth_system_) {
    PA_LOG(VERBOSE) << "Creating ProximityAuthSystem.";
    proximity_auth_system_ =
        std::make_unique<proximity_auth::ProximityAuthSystem>(
            proximity_auth_client(), secure_channel_client_);
  }

  proximity_auth_system_->SetRemoteDevicesForUser(account_id, remote_devices,
                                                  local_device);
  proximity_auth_system_->Start();
}

void EasyUnlockService::PrepareForSuspend() {
  if (base::FeatureList::IsEnabled(features::kSmartLockUIRevamp)) {
    if (smart_lock_state_ && *smart_lock_state_ != SmartLockState::kInactive) {
      ShowInitialSmartLockState();
    }
  } else {
    if (smartlock_state_handler_ && smartlock_state_handler_->IsActive()) {
      UpdateSmartLockState(SmartLockState::kConnectingToPhone);
    }
  }

  if (proximity_auth_system_)
    proximity_auth_system_->OnSuspend();
}

void EasyUnlockService::OnSuspendDone() {
  if (proximity_auth_system_)
    proximity_auth_system_->OnSuspendDone();
}

bool EasyUnlockService::IsSmartLockStateValidOnRemoteAuthFailure() const {
  // Note that NO_PHONE is not valid in this case because the phone may close
  // the connection if the auth challenge sent to it is invalid. This case
  // should be handled as authentication failure.
  return smart_lock_state_ == SmartLockState::kInactive ||
         smart_lock_state_ == SmartLockState::kDisabled ||
         smart_lock_state_ == SmartLockState::kBluetoothDisabled ||
         smart_lock_state_ == SmartLockState::kPhoneFoundLockedAndProximate;
}

void EasyUnlockService::NotifySmartLockAuthResult(bool success) {
  if (!proximity_auth::ScreenlockBridge::Get()->IsLocked())
    return;

  proximity_auth::ScreenlockBridge::Get()
      ->lock_handler()
      ->NotifySmartLockAuthResult(GetAccountId(), success);
}

std::string EasyUnlockService::GetLastRemoteStatusUnlockForLogging() {
  if (proximity_auth_system_) {
    return proximity_auth_system_->GetLastRemoteStatusUnlockForLogging();
  }
  return std::string();
}

const multidevice::RemoteDeviceRefList
EasyUnlockService::GetRemoteDevicesForTesting() const {
  if (!proximity_auth_system_) {
    return multidevice::RemoteDeviceRefList();
  }

  return proximity_auth_system_->GetRemoteDevicesForUser(GetAccountId());
}

}  // namespace ash
