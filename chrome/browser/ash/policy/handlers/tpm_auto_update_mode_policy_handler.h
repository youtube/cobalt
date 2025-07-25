// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_TPM_AUTO_UPDATE_MODE_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_TPM_AUTO_UPDATE_MODE_POLICY_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/notifications/tpm_auto_update_notification.h"
#include "chrome/browser/ash/settings/cros_settings.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class OneShotTimer;
}

namespace policy {

// Enum that corresponds to the possible values of the device policy key
// TPMFirmwareUpdateSettings.AutoUpdateMode.
enum class AutoUpdateMode {
  // Never force update TPM firmware.
  kNever = 1,
  // Update the TPM firmware at the next reboot after user acknowledgment.
  kUserAcknowledgment = 2,
  // Foce update the TPM firmware at the next reboot.
  kWithoutAcknowledgment = 3,
  // Update the TPM firmware after enrollment.
  kEnrollment = 4
};

// This class observes the device setting |kTPMFirmwareUpdateSettings| and
// starts the TPM firmware auto-update flow according to its value.
class TPMAutoUpdateModePolicyHandler {
 public:
  // Will be invoked by TPMAutoUpdateModePolicyHandler to check whether a TPM
  // update is available.
  // The passed |callback| is expected to be called with true if a TPM update is
  // available and with false if no TPM update is available.
  using UpdateCheckerCallback =
      base::RepeatingCallback<void(base::OnceCallback<void(bool)> callback)>;

  // Will be invoked by TPMAutoUpdateModePolicyHandler to display a notification
  // informing the user that a TPM update which will clear user data is planned
  // in 24 hours or at next reboot, depending on |notification_type|.
  using ShowNotificationCallback = base::RepeatingCallback<void(
      ash::TpmAutoUpdateUserNotification notification_type)>;

  TPMAutoUpdateModePolicyHandler(ash::CrosSettings* cros_settings,
                                 PrefService* local_state);

  TPMAutoUpdateModePolicyHandler(const TPMAutoUpdateModePolicyHandler&) =
      delete;
  TPMAutoUpdateModePolicyHandler& operator=(
      const TPMAutoUpdateModePolicyHandler&) = delete;

  ~TPMAutoUpdateModePolicyHandler();

  // Sets a UpdateCheckerCallback for testing.
  void SetUpdateCheckerCallbackForTesting(
      const UpdateCheckerCallback& callback);

  void SetShowNotificationCallbackForTesting(
      const ShowNotificationCallback& callback);

  void SetNotificationTimerForTesting(
      std::unique_ptr<base::OneShotTimer> timer);

  // Updates the TPM firmware if the device is set to update at enrollment via
  // device policy option TPMFirmwareUpdateSettings.AutoUpdateMode. If the TPM
  // firmware is not vulnerable the method will return without updating.
  void UpdateOnEnrollmentIfNeeded();

  // Shows notifications informing the user about a planned auto-update that
  // will clear user data. Notifications are shown only if the TPM firmware
  // needs to be updated and the device policy
  // TPMFirmwareUpdateSettings.AutoUpdateMode is set to |UserAcknowledgment|.
  void ShowTPMAutoUpdateNotificationIfNeeded();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  void OnPolicyChanged();

  // Called by the TPM firmware update availability checker via
  // |UpdateCheckerCallback|. Shows TPM auto-update notifications if
  // |update_available| is true.
  void ShowTPMAutoUpdateNotification(bool update_available);

  static void OnUpdateAvailableCheckResult(bool update_available);

  // Check if a TPM firmware update is available.
  void CheckForUpdate(base::OnceCallback<void(bool)> callback);

  bool WasTPMUpdateOnNextRebootNotificationShown();

  void ShowTPMUpdateOnNextRebootNotification();

  raw_ptr<ash::CrosSettings, DanglingUntriaged | ExperimentalAsh>
      cros_settings_;

  raw_ptr<PrefService, DanglingUntriaged | ExperimentalAsh> local_state_;

  std::unique_ptr<base::OneShotTimer> notification_timer_;

  base::CallbackListSubscription policy_subscription_;

  UpdateCheckerCallback update_checker_callback_;

  ShowNotificationCallback show_notification_callback_;

  base::WeakPtrFactory<TPMAutoUpdateModePolicyHandler> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_TPM_AUTO_UPDATE_MODE_POLICY_HANDLER_H_
