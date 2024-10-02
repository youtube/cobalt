// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CREDENTIAL_LEAK_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CREDENTIAL_LEAK_CONTROLLER_ANDROID_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "url/gurl.h"

namespace ui {
class WindowAndroid;
}

class CredentialLeakDialogViewAndroid;

// Class which manages the dialog displayed when a credential leak was
// detected. It is self-owned and it owns the dialog view.
class CredentialLeakControllerAndroid {
 public:
  CredentialLeakControllerAndroid(
      password_manager::CredentialLeakType leak_type,
      const GURL& origin,
      const std::u16string& username,
      ui::WindowAndroid* window_android,
      // Metrics recorder for leak dialog related UMA and UKM logging.
      std::unique_ptr<password_manager::metrics_util::LeakDialogMetricsRecorder>
          metrics_recorder);

  CredentialLeakControllerAndroid(const CredentialLeakControllerAndroid&) =
      delete;
  CredentialLeakControllerAndroid& operator=(
      const CredentialLeakControllerAndroid&) = delete;

  ~CredentialLeakControllerAndroid();

  // Called when a leaked credential was detected.
  void ShowDialog();

  // Called from the UI when the "Close" button was pressed.
  // Will destroy the controller.
  void OnCancelDialog();

  // Called from the UI when the okay or password check button was pressed.
  // Will destroy the controller.
  void OnAcceptDialog();

  // Called from the UI when the dialog was dismissed by other means (e.g. back
  // button).
  // Will destroy the controller.
  void OnCloseDialog();

  // The label of the accept button. Varies by leak type.
  std::u16string GetAcceptButtonLabel() const;

  // The label of the cancel button. Varies by leak type.
  std::u16string GetCancelButtonLabel() const;

  // Text explaining the leak details. Varies by leak type.
  std::u16string GetDescription() const;

  // The title of the dialog displaying the leak warning.
  std::u16string GetTitle() const;

  // Checks whether the cancel button should be shown.
  bool ShouldShowCancelButton() const;

 private:
  // Used to customize the UI.
  const password_manager::CredentialLeakType leak_type_;

  const GURL origin_;

  const std::u16string username_;

  const raw_ptr<ui::WindowAndroid> window_android_;

  std::unique_ptr<CredentialLeakDialogViewAndroid> dialog_view_;

  std::unique_ptr<password_manager::LeakDialogTraits> leak_dialog_traits_;

  // Metrics recorder for leak dialog related UMA and UKM logging.
  std::unique_ptr<password_manager::metrics_util::LeakDialogMetricsRecorder>
      metrics_recorder_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CREDENTIAL_LEAK_CONTROLLER_ANDROID_H_
