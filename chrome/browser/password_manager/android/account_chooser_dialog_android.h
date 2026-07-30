// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_

#include <stddef.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

// Native counterpart for the android dialog which allows users to select
// credentials which will be passed to the web site in order to log in the user.
class AccountChooserDialogAndroid : public content::WebContentsObserver {
 public:
  AccountChooserDialogAndroid(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials,
      const url::Origin& origin,
      ManagePasswordsState::CredentialsCallback callback);

  AccountChooserDialogAndroid(const AccountChooserDialogAndroid&) = delete;
  AccountChooserDialogAndroid& operator=(const AccountChooserDialogAndroid&) =
      delete;

  ~AccountChooserDialogAndroid() override;
  // Returns true if the dialog is shown. Otherwise, the instance is deleted.
  bool ShowDialog();

  // Closes the dialog and propagates that no credentials was chosen.
  // Destroys |this|.
  void CancelDialog(JNIEnv* env);

  // Propagates the credentials chosen by the user.
  // Results in |this| being destroyed only when the credential handling
  // finishes.
  void OnCredentialClicked(JNIEnv* env,
                           int32_t credential_item,
                           bool sign_button_clicked);

  // content::WebContentsObserver overrides:
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  void OnDialogCancel();

  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  local_credentials_forms() const;

  // Returns whether the credential handling has finished or not. If true,
  // |this| is no longer needed and can be destroyed. If re-authentication is
  // required, the handling is not considered done until that finishes.
  bool HandleCredentialChosen(size_t index, bool sign_button_clicked);

  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // Client used to retrieve the biometric authenticator.
  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  ManagePasswordsState passwords_data_;
  url::Origin origin_;
  base::android::ScopedJavaGlobalRef<jobject> dialog_jobject_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_
