// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/account_chooser_dialog_android.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/visibility.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Pointee;

password_manager::PasswordFormData kFormData1 = {
    password_manager::PasswordForm::Scheme::kHtml,
    "http://example.com/",
    "http://example.com/origin",
    "http://example.com/action",
    u"submit_element",
    u"username_element",
    u"password_element",
    u"",
    u"",
    true,
    1,
};

password_manager::PasswordFormData kFormData2 = {
    password_manager::PasswordForm::Scheme::kHtml,
    "http://test.com/",
    "http://test.com/origin",
    "http://test.com/action",
    u"submit_element",
    u"username_element",
    u"password_element",
    u"",
    u"",
    true,
    1,
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {};

}  // namespace

class AccountChooserDialogAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  AccountChooserDialogAndroidTest();

  AccountChooserDialogAndroidTest(const AccountChooserDialogAndroidTest&) =
      delete;
  AccountChooserDialogAndroidTest& operator=(
      const AccountChooserDialogAndroidTest&) = delete;

  ~AccountChooserDialogAndroidTest() override = default;

  void SetUp() override;

 protected:
  AccountChooserDialogAndroid* CreateDialogManyAccounts();

  AccountChooserDialogAndroid* CreateDialog(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials);

  MockPasswordManagerClient client_;

  base::MockCallback<ManagePasswordsState::CredentialsCallback>
      credential_callback_;

};

AccountChooserDialogAndroidTest::AccountChooserDialogAndroidTest() = default;

void AccountChooserDialogAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
}

AccountChooserDialogAndroid* AccountChooserDialogAndroidTest::CreateDialog(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials) {
  return new AccountChooserDialogAndroid(
      web_contents(), &client_, std::move(credentials),
      url::Origin::Create(GURL("https://example.com")),
      credential_callback_.Get());
}

AccountChooserDialogAndroid*
AccountChooserDialogAndroidTest::CreateDialogManyAccounts() {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> credentials;
  credentials.push_back(
      FillPasswordFormWithData(kFormData1, /*is_account_store=*/false));
  credentials.push_back(
      FillPasswordFormWithData(kFormData2, /*is_account_store=*/false));
  return CreateDialog(std::move(credentials));
}

TEST_F(AccountChooserDialogAndroidTest, SendsCredentialClick) {
  AccountChooserDialogAndroid* dialog = CreateDialogManyAccounts();

  std::unique_ptr<password_manager::PasswordForm> form =
      FillPasswordFormWithData(kFormData2, /*is_account_store=*/false);

  EXPECT_CALL(credential_callback_, Run(Pointee(*form.get())));

  dialog->OnCredentialClicked(base::android::AttachCurrentThread(),
                              1 /* credential_item */,
                              false /* signin_button_clicked */);
}
