// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_

#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

// Controller for SuccessfulPasswordChangeView which is displayed after
// successful password change.
class SuccessfulPasswordChangeBubbleController
    : public PasswordBubbleControllerBase {
 public:
  explicit SuccessfulPasswordChangeBubbleController(
      base::WeakPtr<PasswordsModelDelegate> delegate);

  ~SuccessfulPasswordChangeBubbleController() override;

  // PasswordBubbleControllerBase methods:
  std::u16string GetTitle() const override;
  void ReportInteractions() override;

  void OpenPasswordManager();
  void FinishPasswordChange();
  void AuthenticateUser(base::OnceCallback<void(bool)> auth_callback);

  std::u16string GetDisplayOrigin() const;
  std::u16string GetUsername() const;
  std::u16string GetNewPassword() const;

  void NavigateToPasswordChangeSettings();

  base::WeakPtr<SuccessfulPasswordChangeBubbleController> GetWeakPtr();

 private:
  base::WeakPtr<PasswordChangeDelegate> password_change_delegate_;
  // Dismissal reason for a password bubble.
  password_manager::metrics_util::UIDismissalReason dismissal_reason_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;

  base::WeakPtrFactory<SuccessfulPasswordChangeBubbleController>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_BUBBLE_CONTROLLERS_PASSWORD_CHANGE_SUCCESSFUL_PASSWORD_CHANGE_BUBBLE_CONTROLLER_H_
