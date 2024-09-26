// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_accessory_controller_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_generation_controller_impl.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/ui/accessory_sheet_enums.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessorySheetField;
using autofill::AccessoryTabType;
using autofill::FooterCommand;
using autofill::UserInfo;
using autofill::mojom::FocusedFieldType;
using base::test::RunOnceCallback;
using device_reauth::DeviceAuthRequester;
using device_reauth::MockDeviceAuthenticator;
using password_manager::CreateEntry;
using password_manager::CredentialCache;
using password_manager::MockPasswordStoreInterface;
using password_manager::OriginCredentialStore;
using password_manager::PasswordForm;
using password_manager::PasswordStoreInterface;
using password_manager::TestPasswordStore;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using FillingSource = ManualFillingController::FillingSource;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;
using IsExactMatch = autofill::UserInfo::IsExactMatch;

constexpr char kExampleSite[] = "https://example.com";
constexpr char kExampleHttpSite[] = "http://example.com";
constexpr char16_t kExampleHttpSite16[] = u"http://example.com";
constexpr char kExampleSiteMobile[] = "https://m.example.com";
constexpr char kExampleSignonRealm[] = "https://example.com/";
constexpr char16_t kExampleDomain[] = u"example.com";
constexpr char16_t kUsername[] = u"alice";
constexpr char16_t kPassword[] = u"password123";

class MockPasswordGenerationController
    : public PasswordGenerationControllerImpl {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  explicit MockPasswordGenerationController(content::WebContents* web_contents);

  MOCK_METHOD(void,
              OnGenerationRequested,
              (autofill::password_generation::PasswordGenerationType));
};

// static
void MockPasswordGenerationController::CreateForWebContents(
    content::WebContents* web_contents) {
  ASSERT_FALSE(FromWebContents(web_contents));
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new MockPasswordGenerationController(web_contents)));
}

MockPasswordGenerationController::MockPasswordGenerationController(
    content::WebContents* web_contents)
    : PasswordGenerationControllerImpl(web_contents) {}

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  explicit MockPasswordManagerClient(PasswordStoreInterface* password_store)
      : password_store_(password_store) {}

  MOCK_METHOD(void, UpdateFormManagers, (), (override));

  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));

  MOCK_METHOD(scoped_refptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));

  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override {
    return password_store_;
  }

 private:
  raw_ptr<PasswordStoreInterface> password_store_;
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const std::u16string&),
              (override));
};

std::u16string password_for_str(const std::u16string& user) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, user);
}

std::u16string passwords_empty_str(const std::u16string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE, domain);
}

std::u16string passwords_title_str(const std::u16string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE, domain);
}

std::u16string no_user_str() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
}

std::u16string show_other_passwords_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_SELECT_PASSWORD);
}

std::u16string manage_passwords_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
}

std::u16string generate_password_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_GENERATE_PASSWORD_BUTTON_TITLE);
}

// Creates a AccessorySheetDataBuilder object with a "Manage passwords..."
// footer.
AccessorySheetData::Builder PasswordAccessorySheetDataBuilder(
    const std::u16string& title) {
  return AccessorySheetData::Builder(AccessoryTabType::PASSWORDS, title)
      .AppendFooterCommand(manage_passwords_str(),
                           autofill::AccessoryAction::MANAGE_PASSWORDS);
}

PasswordForm MakeSavedPassword() {
  PasswordForm form;
  form.signon_realm = std::string(kExampleSite);
  form.url = GURL(kExampleSite);
  form.username_value = kUsername;
  form.password_value = kPassword;
  form.username_element = u"";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

class PasswordAccessoryControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordAccessoryControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    ASSERT_TRUE(web_contents()->GetFocusedFrame());
    ASSERT_EQ(url::Origin::Create(GURL(kExampleSite)),
              web_contents()->GetFocusedFrame()->GetLastCommittedOrigin());

    MockPasswordGenerationController::CreateForWebContents(web_contents());
    mock_pwd_manager_client_ = std::make_unique<MockPasswordManagerClient>(
        CreateInternalPasswordStore());
    NavigateAndCommit(GURL(kExampleSite));
  }

  void CreateSheetController(
      security_state::SecurityLevel security_level = security_state::SECURE) {
    PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), cache(), mock_manual_filling_controller_.AsWeakPtr(),
        mock_pwd_manager_client_.get(),
        base::BindRepeating(&PasswordAccessoryControllerTest::GetBaseDriver,
                            base::Unretained(this)));

    controller()->RegisterFillingSourceObserver(filling_source_observer_.Get());
    controller()->SetSecurityLevelForTesting(security_level);
  }

  PasswordAccessoryControllerImpl* controller() {
    return PasswordAccessoryControllerImpl::FromWebContents(web_contents());
  }

  password_manager::CredentialCache* cache() { return &credential_cache_; }

  MockPasswordManagerClient* password_client() {
    return mock_pwd_manager_client_.get();
  }

  MockPasswordManagerDriver* driver() { return &mock_driver_; }

 protected:
  virtual PasswordStoreInterface* CreateInternalPasswordStore() {
    mock_password_store_ = base::MakeRefCounted<MockPasswordStoreInterface>();
    return mock_password_store_.get();
  }

  StrictMock<MockManualFillingController> mock_manual_filling_controller_;
  base::MockCallback<AccessoryController::FillingSourceObserver>
      filling_source_observer_;
  scoped_refptr<MockPasswordStoreInterface> mock_password_store_;
  scoped_refptr<MockDeviceAuthenticator> mock_authenticator_ =
      base::MakeRefCounted<MockDeviceAuthenticator>();

 private:
  password_manager::PasswordManagerDriver* GetBaseDriver(
      content::WebContents*) {
    return driver();
  }

  password_manager::CredentialCache credential_cache_;
  std::unique_ptr<MockPasswordManagerClient> mock_pwd_manager_client_;
  MockPasswordManagerDriver mock_driver_;
};

TEST_F(PasswordAccessoryControllerTest, IsNotRecreatedForSameWebContents) {
  CreateSheetController();
  PasswordAccessoryControllerImpl* initial_controller =
      PasswordAccessoryControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  PasswordAccessoryControllerImpl::CreateForWebContents(web_contents(),
                                                        cache());
  EXPECT_EQ(PasswordAccessoryControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(PasswordAccessoryControllerTest, TransformsMatchesToSuggestions) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(u"Ben", u"Ben", false, true)
              .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, HintsToEmptyUserNames) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(no_user_str(), no_user_str(), false, false)
              .AppendField(u"S3cur3", password_for_str(no_user_str()), true,
                           false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, SortsAlphabeticalDuringTransform) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get(),
       CreateEntry("Zebra", "M3h", GURL(kExampleSite), false, false).get(),
       CreateEntry("Alf", "PWD", GURL(kExampleSite), false, false).get(),
       CreateEntry("Cat", "M1@u", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  AccessorySheetData result(AccessoryTabType::PASSWORDS, std::u16string());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(
      result,
      PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
          .AddUserInfo(kExampleSite)
          .AppendField(u"Alf", u"Alf", false, true)
          .AppendField(u"PWD", password_for_str(u"Alf"), true, false)
          .AddUserInfo(kExampleSite)
          .AppendField(u"Ben", u"Ben", false, true)
          .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
          .AddUserInfo(kExampleSite)
          .AppendField(u"Cat", u"Cat", false, true)
          .AppendField(u"M1@u", password_for_str(u"Cat"), true, false)
          .AddUserInfo(kExampleSite)
          .AppendField(u"Zebra", u"Zebra", false, true)
          .AppendField(u"M3h", password_for_str(u"Zebra"), true, false)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, RepeatsSuggestionsForSameFrame) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  // Pretend that any input in the same frame was focused.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(u"Ben", u"Ben", false, true)
              .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, ProvidesEmptySuggestionsMessage) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestions(PasswordAccessorySheetDataBuilder(
                                     passwords_empty_str(kExampleDomain))
                                     .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, PasswordFieldChangesSuggestionType) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get(),
       CreateEntry("", "p455w0rd", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  // Pretend a username field was focused. This should result in non-interactive
  // suggestion.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(u"No username", u"No username", false, false)
              .AppendField(u"p455w0rd", password_for_str(u"No username"), true,
                           false)
              .AddUserInfo(kExampleSite)
              .AppendField(u"Ben", u"Ben", false, true)
              .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  // Pretend that we focus a password field now: By triggering a refresh with
  // |is_password_field| set to true, all suggestions other than the empty
  // username should become interactive.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(u"No username", u"No username", false, false)
              .AppendField(u"p455w0rd", password_for_str(u"No username"), true,
                           true)
              .AddUserInfo(kExampleSite)
              .AppendField(u"Ben", u"Ben", false, true)
              .AppendField(u"S3cur3", password_for_str(u"Ben"), true, true)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, CachesIsReplacedByNewPasswords) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(u"Ben", u"Ben", false, true)
              .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Alf", "M3lm4k", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(u"Alf", u"Alf", false, true)
              .AppendField(u"M3lm4k", password_for_str(u"Alf"), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, HidesEntriesForPSLMatchedOriginsInV1) {
  CreateSheetController();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      autofill::features::kAutofillKeyboardAccessory);
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get(),
       CreateEntry("Alf", "R4nd0m", GURL(kExampleSiteMobile), true, false)
           .get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  AccessorySheetData result(AccessoryTabType::PASSWORDS, std::u16string());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillOnce(SaveArg<0>(&result));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(
      result,
      PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
          .AddUserInfo(kExampleSite)
          .AppendField(u"Ben", u"Ben",
                       /*is_obfuscated=*/false, /*selectable=*/true)
          .AppendField(u"S3cur3", password_for_str(u"Ben"),
                       /*is_obfuscated=*/true, /*selectable=*/false)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, SetsTitleForPSLMatchedOriginsInV2) {
  CreateSheetController();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillKeyboardAccessory);
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get(),
       CreateEntry("Alf", "R4nd0m", GURL(kExampleSiteMobile), true, false)
           .get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
          .AddUserInfo(kExampleSite)
          .AppendField(u"Ben", u"Ben",
                       /*is_obfuscated=*/false, /*selectable=*/true)
          .AppendField(u"S3cur3", password_for_str(u"Ben"),
                       /*is_obfuscated=*/true, /*selectable=*/false)
          .AddUserInfo(kExampleSiteMobile, IsExactMatch(false))
          .AppendField(u"Alf", u"Alf",
                       /*is_obfuscated=*/false, /*selectable=*/true)
          .AppendField(u"R4nd0m", password_for_str(u"Alf"),
                       /*is_obfuscated=*/true, /*selectable=*/false)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, UnfillableFieldClearsSuggestions) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  // Pretend a username field was focused. This should result in non-emtpy
  // suggestions.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(u"Ben", u"Ben", false, true)
              .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  // Pretend that the focus was lost or moved to an unfillable field. Now, only
  // the empty state message should be sent.
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestions(PasswordAccessorySheetDataBuilder(
                                     passwords_empty_str(kExampleDomain))
                                     .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kUnfillableElement,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, NavigatingMainFrameClearsSuggestions) {
  CreateSheetController();
  // Set any, non-empty password list and pretend a username field was focused.
  // This should result in non-emtpy suggestions.
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(
          PasswordAccessorySheetDataBuilder(passwords_title_str(kExampleDomain))
              .AddUserInfo(kExampleSite)
              .AppendField(u"Ben", u"Ben", false, true)
              .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
              .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  // Pretend that the focus was lost or moved to an unfillable field.
  NavigateAndCommit(GURL("https://random.other-site.org/"));

  // Now, only the empty state message should be sent.
  EXPECT_CALL(
      mock_manual_filling_controller_,
      RefreshSuggestions(PasswordAccessorySheetDataBuilder(
                             passwords_empty_str(u"random.other-site.org"))
                             .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kUnfillableElement,
      /*is_manual_generation_available=*/false);
}

TEST_F(PasswordAccessoryControllerTest, OnAutomaticGenerationRequested) {
  CreateSheetController();
  MockPasswordGenerationController* mock_pwd_generation_controller =
      static_cast<MockPasswordGenerationController*>(
          PasswordGenerationController::GetIfExisting(web_contents()));
  EXPECT_CALL(
      *mock_pwd_generation_controller,
      OnGenerationRequested(
          autofill::password_generation::PasswordGenerationType::kAutomatic));
  controller()->OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType::kAutomatic);
}

TEST_F(PasswordAccessoryControllerTest, AddsGenerationCommandWhenAvailable) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  AccessorySheetData::Builder data_builder(AccessoryTabType::PASSWORDS,
                                           passwords_empty_str(kExampleDomain));
  data_builder
      .AppendFooterCommand(generate_password_str(),
                           autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL)
      .AppendFooterCommand(manage_passwords_str(),
                           autofill::AccessoryAction::MANAGE_PASSWORDS);
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestions(std::move(data_builder).Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/true);
}

TEST_F(PasswordAccessoryControllerTest, NoGenerationCommandIfNotPasswordField) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(mock_manual_filling_controller_,
              RefreshSuggestions(PasswordAccessorySheetDataBuilder(
                                     passwords_empty_str(kExampleDomain))
                                     .Build()));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/true);
}

TEST_F(PasswordAccessoryControllerTest, OnManualGenerationRequested) {
  CreateSheetController();
  MockPasswordGenerationController* mock_pwd_generation_controller =
      static_cast<MockPasswordGenerationController*>(
          PasswordGenerationController::GetIfExisting(web_contents()));
  EXPECT_CALL(mock_manual_filling_controller_, Hide());
  EXPECT_CALL(
      *mock_pwd_generation_controller,
      OnGenerationRequested(
          autofill::password_generation::PasswordGenerationType::kManual));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}

TEST_F(PasswordAccessoryControllerTest, AddsSaveToggleIfIsBlocklisted) {
  CreateSheetController();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory},
      {});
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));
  AccessorySheetData::Builder data_builder(AccessoryTabType::PASSWORDS,
                                           passwords_empty_str(kExampleDomain));
  data_builder
      .SetOptionToggle(
          l10n_util::GetStringUTF16(IDS_PASSWORD_SAVING_STATUS_TOGGLE), false,
          autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS)
      .AppendFooterCommand(manage_passwords_str(),
                           autofill::AccessoryAction::MANAGE_PASSWORDS);

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(controller()->GetSheetData(), std::move(data_builder).Build());
}

TEST_F(PasswordAccessoryControllerTest,
       NoSaveToggleIfIsBlocklistedAndSavingDisabled) {
  CreateSheetController();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory},
      {});

  // Simulate saving being disabled (e.g. being in incognito or having password
  // saving disabled from settings).
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(false));

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));

  AccessorySheetData::Builder data_builder(AccessoryTabType::PASSWORDS,
                                           passwords_empty_str(kExampleDomain));
  data_builder.AppendFooterCommand(manage_passwords_str(),
                                   autofill::AccessoryAction::MANAGE_PASSWORDS);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(controller()->GetSheetData(), std::move(data_builder).Build());
}

TEST_F(PasswordAccessoryControllerTest, AddsSaveToggleIfWasBlocklisted) {
  CreateSheetController();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory},
      {});
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  // Simulate unblocklisting.
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));
  AccessorySheetData::Builder data_builder(AccessoryTabType::PASSWORDS,
                                           passwords_empty_str(kExampleDomain));
  data_builder
      .SetOptionToggle(
          l10n_util::GetStringUTF16(IDS_PASSWORD_SAVING_STATUS_TOGGLE), true,
          autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS)
      .AppendFooterCommand(manage_passwords_str(),
                           autofill::AccessoryAction::MANAGE_PASSWORDS);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(controller()->GetSheetData(), std::move(data_builder).Build());
}

TEST_F(PasswordAccessoryControllerTest, AddsSaveToggleOnAnyFieldIfBlocked) {
  CreateSheetController();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory},
      {});
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));
  AccessorySheetData::Builder data_builder(AccessoryTabType::PASSWORDS,
                                           passwords_empty_str(kExampleDomain));
  data_builder
      .SetOptionToggle(
          l10n_util::GetStringUTF16(IDS_PASSWORD_SAVING_STATUS_TOGGLE), false,
          autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS)
      .AppendFooterCommand(manage_passwords_str(),
                           autofill::AccessoryAction::MANAGE_PASSWORDS);
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableNonSearchField,
      /*is_manual_generation_available=*/false);

  EXPECT_EQ(controller()->GetSheetData(), std::move(data_builder).Build());
}

TEST_F(PasswordAccessoryControllerTest,
       RecordsAccessoryImpressionsForBlocklisted) {
  CreateSheetController();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory},
      {});

  base::HistogramTester histogram_tester;

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.DisabledSavingAccessoryImpressions", true, 1);
}

TEST_F(PasswordAccessoryControllerTest, NoAccessoryImpressionsIfUnblocklisted) {
  CreateSheetController();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory},
      {});
  base::HistogramTester histogram_tester;

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  // Simulate unblocklisting.
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  histogram_tester.ExpectTotalCount(
      "KeyboardAccessory.DisabledSavingAccessoryImpressions", 0);
}

TEST_F(PasswordAccessoryControllerTest, SavePasswordsToggledUpdatesCache) {
  CreateSheetController();
  url::Origin example_origin = url::Origin::Create(GURL(kExampleSite));
  EXPECT_CALL(*password_client(), UpdateFormManagers);
  controller()->OnToggleChanged(
      autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS, true);
}

TEST_F(PasswordAccessoryControllerTest, SavePasswordsEnabledUpdatesStore) {
  CreateSheetController();
  password_manager::PasswordFormDigest form_digest(
      PasswordForm::Scheme::kHtml, kExampleSignonRealm, GURL(kExampleSite));
  EXPECT_CALL(*mock_password_store_, Unblocklist(form_digest, _));
  controller()->OnToggleChanged(
      autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS, true);
}

TEST_F(PasswordAccessoryControllerTest, SavePasswordsDisabledUpdatesStore) {
  CreateSheetController();
  PasswordForm expected_form;
  expected_form.blocked_by_user = true;
  expected_form.scheme = PasswordForm::Scheme::kHtml;
  expected_form.signon_realm = kExampleSignonRealm;
  expected_form.url = GURL(kExampleSite);
  expected_form.date_created = base::Time::Now();
  EXPECT_CALL(*mock_password_store_, AddLogin(Eq(expected_form), _));
  controller()->OnToggleChanged(
      autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS, false);
}

TEST_F(PasswordAccessoryControllerTest, FillsUsername) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory},
      {});
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  AccessorySheetField selected_field(
      /*display_text=*/u"Ben", /*text_to_fill=*/u"Ben",
      /*a11y_description=*/u"Ben", /*id=*/"", /*is_obfuscated=*/false,
      /*selectable=*/true);
  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())));
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, FillsPasswordIfNoAuthAvailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory,
       password_manager::features::kBiometricTouchToFill},
      {});
  CreateSheetController();

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  AccessorySheetField selected_field(
      /*display_text=*/u"S3cur3", /*text_to_fill=*/u"S3cur3",
      /*a11y_description=*/u"S3cur3", /*id=*/"", /*is_obfuscated=*/true,
      /*selectable=*/true);
  EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
      .WillOnce(Return(mock_authenticator_));
  EXPECT_CALL(*mock_authenticator_.get(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(false));
  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())));
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, FillsPasswordIfAuthSuccessful) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory,
       password_manager::features::kBiometricTouchToFill},
      {});
  CreateSheetController();

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  AccessorySheetField selected_field(
      /*display_text=*/u"S3cur3", /*text_to_fill=*/u"S3cur3",
      /*a11y_description=*/u"S3cur3", /*id=*/"", /*is_obfuscated=*/true,
      /*selectable=*/true);
  ON_CALL(*password_client(), GetDeviceAuthenticator)
      .WillByDefault(Return(mock_authenticator_));
  EXPECT_CALL(*mock_authenticator_.get(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator_.get(),
              Authenticate(DeviceAuthRequester::kFallbackSheet, _,
                           /*use_last_valid_auth=*/true))
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));
  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())));
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, DoesntFillPasswordIfAuthFails) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory,
       password_manager::features::kBiometricTouchToFill},
      {});
  CreateSheetController();

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  AccessorySheetField selected_field(
      /*display_text=*/u"S3cur3", /*text_to_fill=*/u"S3cur3",
      /*a11y_description=*/u"S3cur3", /*id=*/"", /*is_obfuscated=*/true,
      /*selectable=*/true);
  ON_CALL(*password_client(), GetDeviceAuthenticator)
      .WillByDefault(Return(mock_authenticator_));
  EXPECT_CALL(*mock_authenticator_.get(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator_.get(),
              Authenticate(DeviceAuthRequester::kFallbackSheet, _,
                           /*use_last_valid_auth=*/true))
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));
  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())))
      .Times(0);
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, CancelsOngoingAuthIfDestroyed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {password_manager::features::kRecoverFromNeverSaveAndroid,
       autofill::features::kAutofillKeyboardAccessory,
       password_manager::features::kBiometricTouchToFill},
      {});
  CreateSheetController();

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {CreateEntry("Ben", "S3cur3", GURL(kExampleSite), false, false).get()},
      CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  AccessorySheetField selected_field(
      /*display_text=*/u"S3cur3", /*text_to_fill=*/u"S3cur3",
      /*a11y_description=*/u"S3cur3", /*id=*/"", /*is_obfuscated=*/true,
      /*selectable=*/true);
  ON_CALL(*password_client(), GetDeviceAuthenticator)
      .WillByDefault(Return(mock_authenticator_));
  EXPECT_CALL(*mock_authenticator_.get(), CanAuthenticateWithBiometrics)
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_authenticator_.get(),
              Authenticate(DeviceAuthRequester::kFallbackSheet, _,
                           /*use_last_valid_auth=*/true));

  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())))
      .Times(0);
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);

  EXPECT_CALL(*mock_authenticator_.get(),
              Cancel(DeviceAuthRequester::kFallbackSheet));
}

class PasswordAccessoryControllerWithTestStoreTest
    : public PasswordAccessoryControllerTest {
 public:
  TestPasswordStore& test_store() { return *test_store_; }

  void SetUp() override {
    PasswordAccessoryControllerTest::SetUp();
    test_store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  }

  void TearDown() override {
    test_store_->ShutdownOnUIThread();
    task_environment()->RunUntilIdle();
    PasswordAccessoryControllerTest::TearDown();
  }

 protected:
  PasswordStoreInterface* CreateInternalPasswordStore() override {
    test_store_ = CreateAndUseTestPasswordStore(profile());
    return test_store_.get();
  }

 private:
  scoped_refptr<TestPasswordStore> test_store_;
};

TEST_F(PasswordAccessoryControllerWithTestStoreTest,
       AddsShowOtherPasswordsForPasswordField) {
  test_store().AddLogin(MakeSavedPassword());
  task_environment()->RunUntilIdle();
  CreateSheetController();

  // Trigger suggestion refresh(es) and store the latest refresh only.
  AccessorySheetData last_sheet(AccessoryTabType::COUNT, std::u16string());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillRepeatedly(testing::SaveArg<0>(&last_sheet));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.
  EXPECT_EQ(
      last_sheet,
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain))
          .AppendFooterCommand(show_other_passwords_str(),
                               autofill::AccessoryAction::USE_OTHER_PASSWORD)
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerWithTestStoreTest,
       AddsShowOtherPasswordsForUsernameField) {
  test_store().AddLogin(MakeSavedPassword());
  task_environment()->RunUntilIdle();
  CreateSheetController();

  // Trigger suggestion refresh(es) and store the latest refresh only.
  AccessorySheetData last_sheet(AccessoryTabType::COUNT, std::u16string());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillRepeatedly(testing::SaveArg<0>(&last_sheet));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField,
      /*is_manual_generation_available=*/false);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.
  EXPECT_EQ(
      last_sheet,
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain))
          .AppendFooterCommand(show_other_passwords_str(),
                               autofill::AccessoryAction::USE_OTHER_PASSWORD)
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerWithTestStoreTest,
       AddsShowOtherPasswordForOnlyCryptographicSchemeSites) {
  test_store().AddLogin(MakeSavedPassword());
  task_environment()->RunUntilIdle();
  CreateSheetController();
  // `Setup` method sets the URL to https but http is required for this method.
  NavigateAndCommit(GURL(kExampleHttpSite));
  FocusWebContentsOnMainFrame();

  // Trigger suggestion refresh(es) and store the latest refresh only.
  AccessorySheetData last_sheet(AccessoryTabType::COUNT, std::u16string());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillRepeatedly(testing::SaveArg<0>(&last_sheet));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.
  EXPECT_EQ(
      last_sheet,
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleHttpSite16))
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerWithTestStoreTest,
       HideShowOtherPasswordForLowSecurityLevelSites) {
  test_store().AddLogin(MakeSavedPassword());
  task_environment()->RunUntilIdle();
  CreateSheetController(security_state::WARNING);

  // Trigger suggestion refresh(es) and store the latest refresh only.
  AccessorySheetData last_sheet(AccessoryTabType::COUNT, std::u16string());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillRepeatedly(testing::SaveArg<0>(&last_sheet));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.
  EXPECT_EQ(
      last_sheet,
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain))
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerWithTestStoreTest,
       HidesUseOtherPasswordsIfPasswordStoreIsEmpty) {
  CreateSheetController();

  // Trigger suggestion refresh(es) and store the latest refresh only.
  AccessorySheetData last_sheet(AccessoryTabType::COUNT, std::u16string());
  EXPECT_CALL(mock_manual_filling_controller_, RefreshSuggestions)
      .WillRepeatedly(testing::SaveArg<0>(&last_sheet));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField,
      /*is_manual_generation_available=*/false);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.
  EXPECT_EQ(
      last_sheet,
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain))
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}
