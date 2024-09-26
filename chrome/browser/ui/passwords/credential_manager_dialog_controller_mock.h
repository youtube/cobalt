// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_MOCK_H_

#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class CredentialManagerDialogControllerMock
    : public CredentialManagerDialogController {
 public:
  CredentialManagerDialogControllerMock();

  CredentialManagerDialogControllerMock(
      const CredentialManagerDialogControllerMock&) = delete;
  CredentialManagerDialogControllerMock& operator=(
      const CredentialManagerDialogControllerMock&) = delete;

  ~CredentialManagerDialogControllerMock() override;

  MOCK_CONST_METHOD0(GetLocalForms, const FormsVector&());
  MOCK_CONST_METHOD0(GetAccoutChooserTitle, std::u16string());
  MOCK_CONST_METHOD0(ShouldShowSignInButton, bool());
  MOCK_CONST_METHOD0(GetAutoSigninPromoTitle, std::u16string());
  MOCK_CONST_METHOD0(GetAutoSigninText, std::u16string());
  MOCK_CONST_METHOD0(ShouldShowFooter, bool());
  MOCK_METHOD0(OnSmartLockLinkClicked, void());
  MOCK_METHOD2(OnChooseCredentials,
               void(const password_manager::PasswordForm& password_form,
                    password_manager::CredentialType credential_type));
  MOCK_METHOD0(OnSignInClicked, void());
  MOCK_METHOD0(OnAutoSigninOK, void());
  MOCK_METHOD0(OnAutoSigninTurnOff, void());
  MOCK_METHOD0(OnCloseDialog, void());
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_MANAGER_DIALOG_CONTROLLER_MOCK_H_
