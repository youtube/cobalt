// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/android/password_reuse_controller_android.h"

#include <memory>
#include <string>

#include "base/android/build_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

using ReusedPasswordAccountType = safe_browsing::LoginReputationClientRequest::
    PasswordReuseEvent::ReusedPasswordAccountType;

using OnWarningDone = base::OnceCallback<void(WarningAction)>;

using MockOnWarningDone = base::MockCallback<OnWarningDone>;

class PasswordReuseControllerAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordReuseControllerAndroid* MakeController(
      ChromePasswordProtectionService* service,
      ReusedPasswordAccountType password_type,
      OnWarningDone done_callback) {
    // The *Dialog() methods used by the tests below all invoke `delete this;`,
    // thus there is no memory leak here.
    return new PasswordReuseControllerAndroid(
        web_contents(), service, profile()->GetPrefs(), password_type,
        std::move(done_callback));
  }

  void AssertWarningActionEquality(WarningAction expected_action_warning,
                                   WarningAction warning_action) {
    ASSERT_EQ(expected_action_warning, warning_action);
  }
};

TEST_F(PasswordReuseControllerAndroidTest, ClickedIgnore) {
  ReusedPasswordAccountType password_type;

  MakeController(
      nullptr, password_type,
      base::BindOnce(
          &PasswordReuseControllerAndroidTest::AssertWarningActionEquality,
          base::Unretained(this), WarningAction::IGNORE_WARNING))
      ->IgnoreDialog();
}

TEST_F(PasswordReuseControllerAndroidTest, ClickedClose) {
  ReusedPasswordAccountType password_type;

  MakeController(
      nullptr, password_type,
      base::BindOnce(
          &PasswordReuseControllerAndroidTest::AssertWarningActionEquality,
          base::Unretained(this), WarningAction::CLOSE))
      ->CloseDialog();
}

TEST_F(PasswordReuseControllerAndroidTest, VerifyButtonText) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);

  MockOnWarningDone empty_callback;
  ReusedPasswordAccountType password_type;

  PasswordReuseControllerAndroid* controller =
      MakeController(nullptr, password_type, empty_callback.Get());

  {
    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::SAVED_PASSWORD);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(true);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    ReusedPasswordAccountType empty_reused_password;
    controller->SetReusedPasswordAccountTypeForTesting(empty_reused_password);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }

  delete controller;
}

TEST_F(PasswordReuseControllerAndroidTest, VerifyButtonTextOnAutomotive) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should only run on automotive.";
  }
  MockOnWarningDone empty_callback;
  ReusedPasswordAccountType password_type;

  PasswordReuseControllerAndroid* controller =
      MakeController(nullptr, password_type, empty_callback.Get());

  {
    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::SAVED_PASSWORD);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(true);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    ReusedPasswordAccountType empty_reused_password;
    controller->SetReusedPasswordAccountTypeForTesting(empty_reused_password);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }

  delete controller;
}

TEST_F(PasswordReuseControllerAndroidTest,
       VerifyButtonTextLoginDbExportNotDone) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);
  // Password export is only relevant if UPM is not already active.
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kUpmUnmigratedPasswordsExported, false);

  MockOnWarningDone empty_callback;
  ReusedPasswordAccountType password_type;

  PasswordReuseControllerAndroid* controller =
      MakeController(nullptr, password_type, empty_callback.Get());

  {
    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::SAVED_PASSWORD);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(true);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    ReusedPasswordAccountType empty_reused_password;
    controller->SetReusedPasswordAccountTypeForTesting(empty_reused_password);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }

  delete controller;
}

TEST_F(PasswordReuseControllerAndroidTest,
       VerifyButtonTextLoginDbDeprecationUPMActive) {
  // Skipping on automotive, since the regular button text for
  // SAVED_PASSWORD does not apply there.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kLoginDbDeprecationAndroid);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kUpmUnmigratedPasswordsExported, false);

  MockOnWarningDone empty_callback;
  ReusedPasswordAccountType password_type;

  PasswordReuseControllerAndroid* controller =
      MakeController(nullptr, password_type, empty_callback.Get());

  {
    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::SAVED_PASSWORD);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(true);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_PAGE_INFO_PROTECT_ACCOUNT_BUTTON),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON),
        controller->GetSecondaryButtonText());
  }
  {
    ReusedPasswordAccountType empty_reused_password;
    controller->SetReusedPasswordAccountTypeForTesting(empty_reused_password);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }
  {
    password_type.set_account_type(ReusedPasswordAccountType::GMAIL);
    password_type.set_is_account_syncing(false);

    controller->SetReusedPasswordAccountTypeForTesting(password_type);

    ASSERT_EQ(l10n_util::GetStringUTF16(IDS_CLOSE),
              controller->GetPrimaryButtonText());
    ASSERT_EQ(std::u16string(), controller->GetSecondaryButtonText());
  }

  delete controller;
}

TEST_F(PasswordReuseControllerAndroidTest, WebContentDestroyed) {
  base::HistogramTester histograms;
  ReusedPasswordAccountType password_type;

  MakeController(
      nullptr, password_type,
      base::BindOnce(
          &PasswordReuseControllerAndroidTest::AssertWarningActionEquality,
          base::Unretained(this), WarningAction::IGNORE_WARNING));

  DeleteContents();
  // This histogram is logged in the destructor of the controller. If it is
  // logged, it indicates that the controller is properly destroyed after the
  // WebContents is destroyed.
  histograms.ExpectTotalCount("PasswordProtection.ModalWarningDialogLifetime",
                              /*count=*/1);
}

}  // namespace safe_browsing
