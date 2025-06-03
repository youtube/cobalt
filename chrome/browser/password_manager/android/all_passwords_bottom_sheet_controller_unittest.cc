// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"

#include "base/android/build_info.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/core/browser/password_protection/stub_password_reuse_detection_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using autofill::mojom::FocusedFieldType;
using base::test::RunOnceCallback;
using device_reauth::MockDeviceAuthenticator;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::UiCredential;

using CallbackFunctionMock = testing::MockFunction<void()>;

using DismissCallback = base::MockCallback<base::OnceCallback<void()>>;

using RequestsToFillPassword =
    AllPasswordsBottomSheetController::RequestsToFillPassword;

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "http://www.example.org";
constexpr char kExampleDe[] = "https://www.example.de";

constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";

constexpr char16_t kPassword[] = u"password123";

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const std::u16string&),
              (override));
};

class MockAllPasswordsBottomSheetView : public AllPasswordsBottomSheetView {
 public:
  MOCK_METHOD(void,
              Show,
              (const std::vector<std::unique_ptr<PasswordForm>>&,
               FocusedFieldType),
              (override));
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
};

class MockPasswordReuseDetectionManagerClient
    : public safe_browsing::StubPasswordReuseDetectionManagerClient {
 public:
  MOCK_METHOD(void, OnPasswordSelected, (const std::u16string&), (override));
};

}  // namespace

PasswordForm MakeSavedPassword(const std::string& signon_realm,
                               const std::u16string& username) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.username_value = username;
  form.password_value = kPassword;
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

PasswordForm MakePasswordException(const std::string& signon_realm) {
  PasswordForm form;
  form.blocked_by_user = true;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

class AllPasswordsBottomSheetControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  AllPasswordsBottomSheetControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kBiometricTouchToFill);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    store_ = CreateAndUseTestPasswordStore(profile());
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    createAllPasswordsController(FocusedFieldType::kFillablePasswordField);
  }

  void createAllPasswordsController(
      autofill::mojom::FocusedFieldType focused_field_type) {
    std::unique_ptr<MockAllPasswordsBottomSheetView> mock_view_unique_ptr =
        std::make_unique<MockAllPasswordsBottomSheetView>();
    mock_view_ = mock_view_unique_ptr.get();
    all_passwords_controller_ =
        std::make_unique<AllPasswordsBottomSheetController>(
            base::PassKey<AllPasswordsBottomSheetControllerTest>(),
            web_contents(), std::move(mock_view_unique_ptr),
            driver_.AsWeakPtr(), store_.get(), dissmissal_callback_.Get(),
            focused_field_type, mock_pwd_manager_client_.get(),
            mock_pwd_reuse_detection_manager_client_.get(),
            show_migration_warning_callback_.Get());
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  TestPasswordStore& store() { return *store_; }

  MockAllPasswordsBottomSheetView& view() { return *mock_view_; }

  AllPasswordsBottomSheetController* all_passwords_controller() {
    return all_passwords_controller_.get();
  }

  DismissCallback& dismissal_callback() { return dissmissal_callback_; }

  void RunUntilIdle() { task_environment()->RunUntilIdle(); }

  MockPasswordManagerClient& client() {
    return *mock_pwd_manager_client_.get();
  }

  MockPasswordReuseDetectionManagerClient&
  password_reuse_detection_manager_client() {
    return *mock_pwd_reuse_detection_manager_client_.get();
  }

  base::MockCallback<
      AllPasswordsBottomSheetController::ShowMigrationWarningCallback>&
  show_migration_warning_callback() {
    return show_migration_warning_callback_;
  }

 private:
  MockPasswordManagerDriver driver_;
  scoped_refptr<TestPasswordStore> store_;

  raw_ptr<MockAllPasswordsBottomSheetView> mock_view_;
  DismissCallback dissmissal_callback_;
  std::unique_ptr<AllPasswordsBottomSheetController> all_passwords_controller_;
  std::unique_ptr<MockPasswordManagerClient> mock_pwd_manager_client_ =
      std::make_unique<MockPasswordManagerClient>();
  std::unique_ptr<MockPasswordReuseDetectionManagerClient>
      mock_pwd_reuse_detection_manager_client_ =
          std::make_unique<MockPasswordReuseDetectionManagerClient>();
  base::MockCallback<
      AllPasswordsBottomSheetController::ShowMigrationWarningCallback>
      show_migration_warning_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AllPasswordsBottomSheetControllerTest, Show) {
  auto form1 = MakeSavedPassword(kExampleCom, kUsername1);
  auto form2 = MakeSavedPassword(kExampleCom, kUsername2);
  auto form3 = MakeSavedPassword(kExampleOrg, kUsername1);
  auto form4 = MakeSavedPassword(kExampleOrg, kUsername2);

  store().AddLogin(form1);
  store().AddLogin(form2);
  store().AddLogin(form3);
  store().AddLogin(form4);
  // Exceptions are not shown. Sites where saving is disabled still show pwds.
  store().AddLogin(MakePasswordException(kExampleDe));
  store().AddLogin(MakePasswordException(kExampleCom));

  EXPECT_CALL(view(),
              Show(UnorderedElementsAre(Pointee(Eq(form1)), Pointee(Eq(form2)),
                                        Pointee(Eq(form3)), Pointee(Eq(form4))),
                   FocusedFieldType::kFillablePasswordField));
  all_passwords_controller()->Show();

  // Show method uses the store which has async work.
  RunUntilIdle();
}

TEST_F(AllPasswordsBottomSheetControllerTest, FillsUsernameWithoutAuth) {
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(client(), GetDeviceAuthenticator).Times(0);
  EXPECT_CALL(driver(),
              FillIntoFocusedField(false, std::u16string(kUsername1)));
  EXPECT_CALL(dismissal_callback(), Run());

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       FillsOnlyUsernameIfNotPasswordFillRequested) {
  EXPECT_CALL(client(), GetDeviceAuthenticator).Times(0);

  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kUsername1)));

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest, FillsPasswordIfAuthNotAvailable) {
  // Auth is required to fill passwords in Android automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }

  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  EXPECT_CALL(*authenticator, CanAuthenticateWithBiometrics)
      .WillOnce(Return(false));
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));
  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kPassword)));
  EXPECT_CALL(dismissal_callback(), Run());

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest, FillsPasswordIfAuthSuccessful) {
  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  ON_CALL(*authenticator, CanAuthenticateWithBiometrics)
      .WillByDefault(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kPassword)));
  EXPECT_CALL(dismissal_callback(), Run());

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest, DoesntFillPasswordIfAuthFailed) {
  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  ON_CALL(*authenticator, CanAuthenticateWithBiometrics)
      .WillByDefault(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kPassword)))
      .Times(0);
  EXPECT_CALL(dismissal_callback(), Run());

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest, CancelsAuthIfDestroyed) {
  auto authenticator = std::make_unique<MockDeviceAuthenticator>();
  auto* authenticator_ptr = authenticator.get();

  ON_CALL(*authenticator, CanAuthenticateWithBiometrics)
      .WillByDefault(Return(true));
  EXPECT_CALL(*authenticator_ptr, AuthenticateWithMessage);
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kPassword)))
      .Times(0);

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));

  EXPECT_CALL(*authenticator_ptr, Cancel());
}

TEST_F(AllPasswordsBottomSheetControllerTest, OnDismiss) {
  EXPECT_CALL(dismissal_callback(), Run());
  all_passwords_controller()->OnDismiss();
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       OnCredentialSelectedTriggersPhishGuard) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    auto authenticator = std::make_unique<MockDeviceAuthenticator>();
    ON_CALL(*authenticator, AuthenticateWithMessage)
        .WillByDefault(RunOnceCallback<1>(/*auth_succeeded=*/true));
    EXPECT_CALL(client(), GetDeviceAuthenticator)
        .WillOnce(Return(testing::ByMove(std::move(authenticator))));
  }

  EXPECT_CALL(password_reuse_detection_manager_client(),
              OnPasswordSelected(std::u16string(kPassword)));

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       PhishGuardIsNotCalledForUsernameInPasswordField) {
  EXPECT_CALL(password_reuse_detection_manager_client(), OnPasswordSelected)
      .Times(0);

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       PhishGuardIsNotCalledForUsername) {
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(password_reuse_detection_manager_client(), OnPasswordSelected)
      .Times(0);

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       FillsUsernameIfPasswordFillRequestedInNonPasswordField) {
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(driver(), FillIntoFocusedField(_, _)).Times(0);

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       ShowMigrationWarningOnUsernameFillIfEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(show_migration_warning_callback(), Run);
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       ShowMigrationWarningOnPasswordFillIfEnabled) {
  // TODO(crbug.com/1484686): Migration warning isn't reached if authenticator
  // is present.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  createAllPasswordsController(FocusedFieldType::kFillablePasswordField);
  EXPECT_CALL(show_migration_warning_callback(),
              Run(_, _,
                  password_manager::metrics_util::
                      PasswordMigrationWarningTriggers::kAllPasswords));
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       DoesntTriggersMigrationWarningIfDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(show_migration_warning_callback(), Run).Times(0);
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}
