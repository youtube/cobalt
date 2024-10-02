// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#import "ios/chrome/browser/passwords/ios_chrome_password_infobar_metrics_recorder.h"

namespace password_manager {
class PasswordFormManagerForUI;
}

// After a successful *new* login attempt, Chrome passes the current
// password_manager::PasswordFormManager and move it to a
// IOSChromeSavePasswordInfoBarDelegate while the user makes up their mind
// with the "save password" infobar.
// If `password_update` is true the delegate will use "Update" related strings,
// and should Update the credentials instead of Saving new ones.
class IOSChromeSavePasswordInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  IOSChromeSavePasswordInfoBarDelegate(
      absl::optional<std::string> account_to_store_password,
      bool password_update,
      password_manager::metrics_util::PasswordAccountStorageUserState
          account_storage_user_state,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save);

  IOSChromeSavePasswordInfoBarDelegate(
      const IOSChromeSavePasswordInfoBarDelegate&) = delete;
  IOSChromeSavePasswordInfoBarDelegate& operator=(
      const IOSChromeSavePasswordInfoBarDelegate&) = delete;

  ~IOSChromeSavePasswordInfoBarDelegate() override;

  // Returns `delegate` as an IOSChromeSavePasswordInfoBarDelegate, or nullptr
  // if it is of another type.
  static IOSChromeSavePasswordInfoBarDelegate* FromInfobarDelegate(
      infobars::InfoBarDelegate* delegate);

  // The Username being saved or updated by the Infobar.
  NSString* GetUserNameText() const;

  // The Password being saved or updated by the Infobar.
  NSString* GetPasswordText() const;

  // The URL host for which the credentials are being saved for.
  NSString* GetURLHostText() const;

  // The account where the password will be saved, or absl::nullopt if it's
  // saved locally.
  absl::optional<std::string> GetAccountToStorePassword() const;

  // InfoBarDelegate implementation.
  bool ShouldExpire(const NavigationDetails& details) const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  void InfoBarDismissed() override;

  // Updates the credentials being saved with `username` and `password`.
  // TODO(crbug.com/1040653): This function is only virtual so it can be mocked
  // for testing purposes.  It should become non-virtual once this test is
  // refactored for testability.
  virtual void UpdateCredentials(NSString* username, NSString* password);

  // Informs the delegate that the Infobar has been presented. If `automatic`
  // YES the Infobar was presented automatically (e.g. The banner was
  // presented), if NO the user triggered it  (e.g. Tapped on the badge).
  // TODO(crbug.com/1040653): This function is only virtual so it can be mocked
  // for testing purposes.  It should become non-virtual once this test is
  // refactored for testability.
  virtual void InfobarPresenting(bool automatic);

  // Informs the delegate that the Infobar has been dismissed.
  // TODO(crbug.com/1040653): This function is only virtual so it can be mocked
  // for testing purposes.  It should become non-virtual once this test is
  // refactored for testability.
  // TODO(crbug.com/1394793): Fix dismissal handlers.
  virtual void InfobarDismissed();

  // True if password is being updated at the moment the InfobarModal is
  // created.
  bool IsPasswordUpdate() const;

  // True if the current set of credentials has already been saved at the moment
  // the InfobarModal is created.
  bool IsCurrentPasswordSaved() const;

 private:
  // ConfirmInfoBarDelegate implementation.
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;

  // The password_manager::PasswordFormManager managing the form we're asking
  // the user about, and should save as per their decision.
  const std::unique_ptr<password_manager::PasswordFormManagerForUI>
      form_to_save_;

  // The PasswordInfobarType for this delegate.
  const PasswordInfobarType infobar_type_;

  // The account where the password will be stored, or absl::nullopt if the
  // password will only be stored on this device.
  const absl::optional<std::string> account_to_store_password_;

  // Used to record metrics related to passwords account storage.
  const password_manager::metrics_util::PasswordAccountStorageUserState
      account_storage_user_state_;

  // Used to track the results we get from the info bar.
  password_manager::metrics_util::UIDismissalReason infobar_response_ =
      password_manager::metrics_util::NO_DIRECT_INTERACTION;

  // True if password is being updated at the moment the InfobarModal is
  // created.
  bool password_update_ = false;

  // True if the current set of credentials has already been saved at the moment
  // the InfobarModal is created.
  bool current_password_saved_ = false;

  // True if an Infobar is being presented by this delegate.
  bool infobar_presenting_ = false;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_SAVE_PASSWORD_INFOBAR_DELEGATE_H_
