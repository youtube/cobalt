// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPDATED_CONFIRMATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPDATED_CONFIRMATION_CONTROLLER_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

// Manages the bubble which is shown as a confirmation when a passkey is
// updated.
class PasskeyUpdatedConfirmationController
    : public PasswordBubbleControllerBase {
 public:
  PasskeyUpdatedConfirmationController(
      base::WeakPtr<PasswordsModelDelegate> delegate,
      password_manager::metrics_util::UIDisplayDisposition display_disposition,
      std::string passkey_rp_id);
  ~PasskeyUpdatedConfirmationController() override;

  // PasswordBubbleControllerBase:
  std::u16string GetTitle() const override;

  // Called by the view when the user clicks the "Google Password Manager" link.
  // Navigates to password manager main page.
  void OnGooglePasswordManagerLinkClicked();

 private:
  // PasswordBubbleControllerBase:
  void ReportInteractions() override;

  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;

  // The relying party identifier of the updated passkey.
  std::string passkey_rp_id_;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPDATED_CONFIRMATION_CONTROLLER_H_
