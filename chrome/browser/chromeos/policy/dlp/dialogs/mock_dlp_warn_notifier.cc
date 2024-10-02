// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dialogs/mock_dlp_warn_notifier.h"

#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;

namespace policy {

MockDlpWarnNotifier::MockDlpWarnNotifier() : should_proceed_(true) {
  // Propagate to the real object.
  ON_CALL(*this, ShowDlpWarningDialog)
      .WillByDefault([this](OnDlpRestrictionCheckedCallback callback,
                            DlpWarnDialog::DlpWarnDialogOptions options,
                            gfx::NativeWindow modal_parent) {
        return this->DlpWarnNotifier::ShowDlpWarningDialog(
            std::move(callback), options, modal_parent);
      });
}

MockDlpWarnNotifier::MockDlpWarnNotifier(bool should_proceed)
    : should_proceed_(should_proceed) {
  // Simulate proceed or cancel.
  ON_CALL(*this, ShowDlpWarningDialog)
      .WillByDefault([this](OnDlpRestrictionCheckedCallback callback,
                            DlpWarnDialog::DlpWarnDialogOptions options,
                            gfx::NativeWindow modal_parent) {
        std::move(callback).Run(should_proceed_);
        return nullptr;
      });
}

MockDlpWarnNotifier::~MockDlpWarnNotifier() = default;

}  // namespace policy
