// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/credential_leak_controller_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#include "chrome/browser/ui/android/passwords/credential_leak_dialog_view_android.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "ui/android/window_android.h"
#include "url/android/gurl_android.h"

using password_manager::CreateDialogTraits;
using password_manager::PasswordCheckReferrerAndroid;
using password_manager::metrics_util::LeakDialogDismissalReason;
using password_manager::metrics_util::LeakDialogMetricsRecorder;
using password_manager::metrics_util::LeakDialogType;

CredentialLeakControllerAndroid::CredentialLeakControllerAndroid(
    password_manager::CredentialLeakType leak_type,
    const GURL& origin,
    const std::u16string& username,
    ui::WindowAndroid* window_android,
    std::unique_ptr<LeakDialogMetricsRecorder> metrics_recorder)
    : leak_type_(leak_type),
      origin_(origin),
      username_(username),
      window_android_(window_android),
      leak_dialog_traits_(CreateDialogTraits(leak_type)),
      metrics_recorder_(std::move(metrics_recorder)) {}

CredentialLeakControllerAndroid::~CredentialLeakControllerAndroid() = default;

void CredentialLeakControllerAndroid::ShowDialog() {
  dialog_view_ = std::make_unique<CredentialLeakDialogViewAndroid>(this);
  dialog_view_->Show(window_android_);
}

void CredentialLeakControllerAndroid::OnCancelDialog() {
  metrics_recorder_->LogLeakDialogTypeAndDismissalReason(
      LeakDialogDismissalReason::kClickedClose);
  delete this;
}

void CredentialLeakControllerAndroid::OnAcceptDialog() {
  LeakDialogType dialog_type = password_manager::GetLeakDialogType(leak_type_);
  LeakDialogDismissalReason dismissal_reason =
      LeakDialogDismissalReason::kClickedOk;
  switch (dialog_type) {
    case LeakDialogType::kChange:
      dismissal_reason = LeakDialogDismissalReason::kClickedOk;
      break;
    case LeakDialogType::kCheckup:
    case LeakDialogType::kCheckupAndChange:
      dismissal_reason = LeakDialogDismissalReason::kClickedCheckPasswords;
      break;
  }

  metrics_recorder_->LogLeakDialogTypeAndDismissalReason(dismissal_reason);

  // |window_android_| might be null in tests.
  if (!window_android_) {
    delete this;
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  switch (dialog_type) {
    case LeakDialogType::kChange:
    case LeakDialogType::kCheckup:
    case LeakDialogType::kCheckupAndChange:
      PasswordCheckupLauncherHelper::LaunchLocalCheckup(
          env, window_android_->GetJavaObject(),
          PasswordCheckReferrerAndroid::kLeakDialog);
      break;
  }

  delete this;
}

void CredentialLeakControllerAndroid::OnCloseDialog() {
  metrics_recorder_->LogLeakDialogTypeAndDismissalReason(
      LeakDialogDismissalReason::kNoDirectInteraction);
  delete this;
}

std::u16string CredentialLeakControllerAndroid::GetAcceptButtonLabel() const {
  return leak_dialog_traits_->GetAcceptButtonLabel();
}

std::u16string CredentialLeakControllerAndroid::GetCancelButtonLabel() const {
  return leak_dialog_traits_->GetCancelButtonLabel();
}

std::u16string CredentialLeakControllerAndroid::GetDescription() const {
  return leak_dialog_traits_->GetDescription();
}

std::u16string CredentialLeakControllerAndroid::GetTitle() const {
  return leak_dialog_traits_->GetTitle();
}

bool CredentialLeakControllerAndroid::ShouldShowCancelButton() const {
  return leak_dialog_traits_->ShouldShowCancelButton();
}
