// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/mock_password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace extensions {

namespace {

constexpr char kExampleCom[] = "https://example.com/";
constexpr char kExampleOrg[] = "http://www.example.org/";
constexpr char kExampleApp[] = "com.example.app";

constexpr char kTestEmail[] = "user@gmail.com";

constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";
constexpr char16_t kUsername3[] = u"eve";

constexpr char16_t kPassword1[] = u"fnlsr4@cm^mdls@fkspnsg3d";
constexpr char16_t kPassword2[] = u"pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";
constexpr char16_t kWeakPassword1[] = u"123456";
constexpr char16_t kWeakPassword2[] = u"111111";

constexpr char kGoogleAccounts[] = "https://accounts.google.com";

using api::passwords_private::CompromisedInfo;
using api::passwords_private::PasswordCheckStatus;
using api::passwords_private::PasswordUiEntry;
using api::passwords_private::UrlCollection;
using password_manager::BulkLeakCheckDelegateInterface;
using password_manager::BulkLeakCheckService;
using password_manager::InsecureType;
using password_manager::InsecurityMetadata;
using password_manager::IsLeaked;
using password_manager::IsMuted;
using password_manager::LeakCheckCredential;
using password_manager::MockPasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTracker;
using password_manager::PasswordChangeSuccessTrackerFactory;
using password_manager::PasswordForm;
using password_manager::SavedPasswordsPresenter;
using password_manager::TestPasswordStore;
using password_manager::TriggerBackendNotification;
using password_manager::prefs::kLastTimePasswordCheckCompleted;
using signin::IdentityTestEnvironment;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using MockStartPasswordCheckCallback =
    base::MockCallback<PasswordCheckDelegate::StartPasswordCheckCallback>;

PasswordsPrivateEventRouter* CreateAndUsePasswordsPrivateEventRouter(
    Profile* profile) {
  return static_cast<PasswordsPrivateEventRouter*>(
      PasswordsPrivateEventRouterFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile,
              base::BindRepeating([](content::BrowserContext* context) {
                return std::unique_ptr<KeyedService>(
                    PasswordsPrivateEventRouter::Create(context));
              })));
}

EventRouter* CreateAndUseEventRouter(Profile* profile) {
  // The factory function only requires that T be a KeyedService. Ensure it is
  // actually derived from EventRouter to avoid undefined behavior.
  return static_cast<EventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindRepeating([](content::BrowserContext* context) {
            return std::unique_ptr<KeyedService>(
                std::make_unique<EventRouter>(context, nullptr));
          })));
}

MockPasswordChangeSuccessTracker* CreateAndUsePasswordChangeSuccessTracker(
    Profile* profile) {
  return static_cast<MockPasswordChangeSuccessTracker*>(
      PasswordChangeSuccessTrackerFactory::GetInstance()
          ->SetTestingSubclassFactoryAndUse(
              profile, base::BindRepeating([](content::BrowserContext*) {
                return std::make_unique<
                    testing::StrictMock<MockPasswordChangeSuccessTracker>>();
              })));
}

BulkLeakCheckService* CreateAndUseBulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    Profile* profile) {
  return static_cast<BulkLeakCheckService*>(
      BulkLeakCheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindLambdaForTesting([identity_manager](
                                                  content::BrowserContext*) {
            return std::unique_ptr<
                KeyedService>(std::make_unique<BulkLeakCheckService>(
                identity_manager,
                base::MakeRefCounted<network::TestSharedURLLoaderFactory>()));
          })));
}

syncer::TestSyncService* CreateAndUseSyncService(Profile* profile) {
  return SyncServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
      profile, base::BindLambdaForTesting([](content::BrowserContext*) {
        return std::make_unique<syncer::TestSyncService>();
      }));
}

PasswordForm MakeSavedPassword(
    base::StringPiece signon_realm,
    base::StringPiece16 username,
    base::StringPiece16 password = kPassword1,
    base::StringPiece16 username_element = u"",
    PasswordForm::Store store = PasswordForm::Store::kProfileStore) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.username_element = std::u16string(username_element);
  form.in_store = store;
  return form;
}

PasswordForm MakeSavedFederatedCredential(
    base::StringPiece signon_realm,
    base::StringPiece16 username,
    base::StringPiece provider = kGoogleAccounts,
    PasswordForm::Store store = PasswordForm::Store::kProfileStore) {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = std::u16string(username);
  form.federation_origin = url::Origin::Create(GURL(provider));
  CHECK(!form.federation_origin.opaque());
  form.in_store = store;
  return form;
}

void AddIssueToForm(PasswordForm* form,
                    InsecureType type,
                    base::TimeDelta time_since_creation = base::TimeDelta(),
                    const bool is_muted = false) {
  form->password_issues.insert_or_assign(
      type,
      InsecurityMetadata(base::Time::Now() - time_since_creation,
                         IsMuted(is_muted), TriggerBackendNotification(false)));
}

std::string MakeAndroidRealm(base::StringPiece package_name) {
  return base::StrCat({"android://hash@", package_name});
}

PasswordForm MakeSavedAndroidPassword(
    base::StringPiece package_name,
    base::StringPiece16 username,
    base::StringPiece app_display_name = "",
    base::StringPiece affiliated_web_realm = "",
    base::StringPiece16 password = kPassword1) {
  PasswordForm form;
  form.signon_realm = MakeAndroidRealm(package_name);
  form.username_value = std::u16string(username);
  form.app_display_name = std::string(app_display_name);
  form.affiliated_web_realm = std::string(affiliated_web_realm);
  form.password_value = std::u16string(password);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

auto ExpectUrls(const std::string& formatted_origin,
                const std::string& detailed_origin) {
  return AllOf(Field(&UrlCollection::shown, formatted_origin),
               Field(&UrlCollection::link, detailed_origin));
}

// Creates matcher for a given compromised info.
auto ExpectCompromisedInfo(
    base::TimeDelta elapsed_time_since_compromise,
    const std::string& elapsed_time_since_compromise_str,
    std::vector<api::passwords_private::CompromiseType> compromise_types) {
  return AllOf(Field(&CompromisedInfo::compromise_time,
                     (base::Time::Now() - elapsed_time_since_compromise)
                         .ToJsTimeIgnoringNull()),
               Field(&CompromisedInfo::elapsed_time_since_compromise,
                     elapsed_time_since_compromise_str),
               Field(&CompromisedInfo::compromise_types,
                     testing::UnorderedElementsAreArray(compromise_types)));
}

// Creates matcher for a given compromised credential
auto ExpectCredential(const std::string& formatted_origin,
                      const std::string& detailed_origin,
                      const absl::optional<std::string>& change_password_url,
                      const std::u16string& username) {
  return AllOf(
      Field(&PasswordUiEntry::username, base::UTF16ToASCII(username)),
      Field(&PasswordUiEntry::urls,
            ExpectUrls(formatted_origin, detailed_origin)),
      Field(&PasswordUiEntry::change_password_url, change_password_url));
}

// Creates matcher for a given compromised credential
auto ExpectCompromisedCredential(
    const std::string& formatted_origin,
    const std::string& detailed_origin,
    const absl::optional<std::string>& change_password_url,
    const std::u16string& username,
    base::TimeDelta elapsed_time_since_compromise,
    const std::string& elapsed_time_since_compromise_str,
    std::vector<api::passwords_private::CompromiseType> compromise_types) {
  auto change_password_url_field_matcher =
      change_password_url.has_value()
          ? Field(&PasswordUiEntry::change_password_url,
                  change_password_url.value())
          : Field(&PasswordUiEntry::change_password_url,
                  testing::Eq(absl::nullopt));
  return AllOf(
      Field(&PasswordUiEntry::username, base::UTF16ToASCII(username)),
      change_password_url_field_matcher,
      Field(&PasswordUiEntry::urls,
            ExpectUrls(formatted_origin, detailed_origin)),
      Field(&PasswordUiEntry::compromised_info,
            Optional(ExpectCompromisedInfo(elapsed_time_since_compromise,
                                          elapsed_time_since_compromise_str,
                                          compromise_types))));
}

class PasswordCheckDelegateTest : public ::testing::Test {
 public:
  PasswordCheckDelegateTest() {
    prefs_.registry()->RegisterDoublePref(kLastTimePasswordCheckCompleted, 0.0);
    presenter_.Init();
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }
  TestEventRouterObserver& event_router_observer() {
    return event_router_observer_;
  }
  IdentityTestEnvironment& identity_test_env() { return identity_test_env_; }
  TestingPrefServiceSimple prefs_;
  TestingProfile& profile() { return profile_; }
  TestPasswordStore& store() { return *store_; }
  TestPasswordStore& account_store() { return *account_store_; }
  BulkLeakCheckService* service() { return bulk_leak_check_service_; }
  MockPasswordChangeSuccessTracker& password_change_success_tracker() {
    return *password_change_success_tracker_;
  }
  syncer::TestSyncService& sync_service() { return *sync_service_; }
  SavedPasswordsPresenter& presenter() { return presenter_; }
  PasswordCheckDelegate& delegate() { return delegate_; }

  PasswordCheckDelegate CreateDelegate(SavedPasswordsPresenter* presenter) {
    return PasswordCheckDelegate(&profile_, presenter,
                                 &credential_id_generator_);
  }

 private:
  content::BrowserTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  TestingProfile profile_;
  raw_ptr<EventRouter> event_router_ = CreateAndUseEventRouter(&profile_);
  raw_ptr<PasswordsPrivateEventRouter> password_router_ =
      CreateAndUsePasswordsPrivateEventRouter(&profile_);
  TestEventRouterObserver event_router_observer_{event_router_};
  raw_ptr<BulkLeakCheckService> bulk_leak_check_service_ =
      CreateAndUseBulkLeakCheckService(identity_test_env_.identity_manager(),
                                       &profile_);
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
  scoped_refptr<TestPasswordStore> account_store_ =
      CreateAndUseTestAccountPasswordStore(&profile_);
  raw_ptr<MockPasswordChangeSuccessTracker> password_change_success_tracker_ =
      CreateAndUsePasswordChangeSuccessTracker(&profile_);
  raw_ptr<syncer::TestSyncService> sync_service_ =
      CreateAndUseSyncService(&profile_);
  IdGenerator credential_id_generator_;
  password_manager::MockAffiliationService affiliation_service_;
  SavedPasswordsPresenter presenter_{&affiliation_service_, store_,
                                     account_store_};
  PasswordCheckDelegate delegate_{&profile_, &presenter_,
                                  &credential_id_generator_};
};

}  // namespace

// Verify that GetInsecureCredentials() correctly represents weak credentials.
TEST_F(PasswordCheckDelegateTest, GetInsecureCredentialsFillsFieldsCorrectly) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  store().AddLogin(MakeSavedAndroidPassword(
      kExampleApp, kUsername2, "Example App", kExampleCom, kWeakPassword2));
  RunUntilIdle();
  delegate().StartPasswordCheck();
  RunUntilIdle();

  EXPECT_THAT(
      delegate().GetInsecureCredentials(),
      UnorderedElementsAre(
          ExpectCredential("example.com", "https://example.com/",
                           "https://example.com/.well-known/change-password",
                           kUsername1),
          ExpectCredential(
              "Example App",
              "https://play.google.com/store/apps/details?id=com.example.app",
              "https://example.com/.well-known/change-password", kUsername2)));
}

// Verify that computation of weak credentials notifies observers.
TEST_F(PasswordCheckDelegateTest, WeakCheckNotifiesObservers) {
  const char* const kEventName =
      api::passwords_private::OnInsecureCredentialsChanged::kEventName;

  // Verify that the event was not fired during construction.
  EXPECT_FALSE(base::Contains(event_router_observer().events(), kEventName));

  // Verify that the event gets fired after weak check is complete.
  delegate().StartPasswordCheck();
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_INSECURE_CREDENTIALS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
}

// Verifies that the weak check will be run if the user is signed out.
TEST_F(PasswordCheckDelegateTest, WeakCheckWhenUserSignedOut) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  RunUntilIdle();
  delegate().StartPasswordCheck();
  RunUntilIdle();

  EXPECT_THAT(
      delegate().GetInsecureCredentials(),
      ElementsAre(ExpectCredential(
          "example.com", "https://example.com/",
          "https://example.com/.well-known/change-password", kUsername1)));
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_SIGNED_OUT,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the formatted timestamp associated with a compromised
// credential covers the "Just now" cases (less than a minute ago), as well as
// months and years.
TEST_F(PasswordCheckDelegateTest, GetInsecureCredentialsHandlesTimes) {
  PasswordForm form_com_username1 = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form_com_username1, InsecureType::kLeaked, base::Seconds(59));
  store().AddLogin(form_com_username1);

  PasswordForm form_com_username2 = MakeSavedPassword(kExampleCom, kUsername2);
  AddIssueToForm(&form_com_username2, InsecureType::kLeaked, base::Seconds(60));
  store().AddLogin(form_com_username2);

  PasswordForm form_org_username1 = MakeSavedPassword(kExampleOrg, kUsername1);
  AddIssueToForm(&form_org_username1, InsecureType::kLeaked, base::Days(100));
  store().AddLogin(form_org_username1);

  PasswordForm form_org_username2 = MakeSavedPassword(kExampleOrg, kUsername2);
  AddIssueToForm(&form_org_username2, InsecureType::kLeaked, base::Days(800));
  store().AddLogin(form_org_username2);

  RunUntilIdle();

  EXPECT_THAT(
      delegate().GetInsecureCredentials(),
      ElementsAre(ExpectCompromisedCredential(
                      "example.com", "https://example.com/",
                      "https://example.com/.well-known/change-password",
                      kUsername1, base::Seconds(59), "Just now",
                      {api::passwords_private::COMPROMISE_TYPE_LEAKED}),
                  ExpectCompromisedCredential(
                      "example.com", "https://example.com/",
                      "https://example.com/.well-known/change-password",
                      kUsername2, base::Seconds(60), "1 minute ago",
                      {api::passwords_private::COMPROMISE_TYPE_LEAKED}),
                  ExpectCompromisedCredential(
                      "example.org", "http://www.example.org/",
                      "http://www.example.org/.well-known/change-password",
                      kUsername1, base::Days(100), "3 months ago",
                      {api::passwords_private::COMPROMISE_TYPE_LEAKED}),
                  ExpectCompromisedCredential(
                      "example.org", "http://www.example.org/",
                      "http://www.example.org/.well-known/change-password",
                      kUsername2, base::Days(800), "2 years ago",
                      {api::passwords_private::COMPROMISE_TYPE_LEAKED})));
}

// Verifies that both leaked and phished credentials are ordered correctly
// within the list of compromised credentials. These credentials should be
// listed before just leaked ones and have a timestamp that corresponds to the
// most recent compromise.
TEST_F(PasswordCheckDelegateTest,
       GetInsecureCredentialsDedupesLeakedAndCompromised) {
  PasswordForm form_com_username1 = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form_com_username1, InsecureType::kLeaked, base::Minutes(1));
  AddIssueToForm(&form_com_username1, InsecureType::kPhished, base::Minutes(5));
  store().AddLogin(form_com_username1);

  PasswordForm form_com_username2 = MakeSavedPassword(kExampleCom, kUsername2);
  AddIssueToForm(&form_com_username2, InsecureType::kLeaked, base::Minutes(2));
  store().AddLogin(form_com_username2);

  PasswordForm form_org_username1 = MakeSavedPassword(kExampleOrg, kUsername1);
  AddIssueToForm(&form_org_username1, InsecureType::kPhished, base::Minutes(3));
  store().AddLogin(form_org_username1);

  PasswordForm form_org_username2 = MakeSavedPassword(kExampleOrg, kUsername2);
  AddIssueToForm(&form_org_username2, InsecureType::kPhished, base::Minutes(4));
  AddIssueToForm(&form_org_username2, InsecureType::kLeaked, base::Minutes(6));
  store().AddLogin(form_org_username2);

  RunUntilIdle();

  EXPECT_THAT(delegate().GetInsecureCredentials(),
              UnorderedElementsAre(
                  ExpectCompromisedCredential(
                      "example.com", "https://example.com/",
                      "https://example.com/.well-known/change-password",
                      kUsername1, base::Minutes(1), "1 minute ago",
                      {api::passwords_private::COMPROMISE_TYPE_LEAKED,
                       api::passwords_private::COMPROMISE_TYPE_PHISHED}),
                  ExpectCompromisedCredential(
                      "example.org", "http://www.example.org/",
                      "http://www.example.org/.well-known/change-password",
                      kUsername1, base::Minutes(3), "3 minutes ago",
                      {api::passwords_private::COMPROMISE_TYPE_PHISHED}),
                  ExpectCompromisedCredential(
                      "example.org", "http://www.example.org/",
                      "http://www.example.org/.well-known/change-password",
                      kUsername2, base::Minutes(4), "4 minutes ago",
                      {api::passwords_private::COMPROMISE_TYPE_LEAKED,
                       api::passwords_private::COMPROMISE_TYPE_PHISHED}),
                  ExpectCompromisedCredential(
                      "example.com", "https://example.com/",
                      "https://example.com/.well-known/change-password",
                      kUsername2, base::Minutes(2), "2 minutes ago",
                      {api::passwords_private::COMPROMISE_TYPE_LEAKED})));
}

TEST_F(PasswordCheckDelegateTest, GetInsecureCredentialsInjectsAndroid) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked, base::Minutes(5));
  store().AddLogin(form);

  PasswordForm android_form1 =
      MakeSavedAndroidPassword(kExampleApp, kUsername1);
  AddIssueToForm(&android_form1, InsecureType::kPhished, base::Days(4));
  store().AddLogin(android_form1);

  PasswordForm android_form2 = MakeSavedAndroidPassword(
      kExampleApp, kUsername2, "Example App", kExampleCom);
  AddIssueToForm(&android_form2, InsecureType::kPhished, base::Days(3));
  store().AddLogin(android_form2);

  RunUntilIdle();

  // Verify that the compromised credentials match what is stored in the
  // password store.
  EXPECT_THAT(
      delegate().GetInsecureCredentials(),
      UnorderedElementsAre(
          ExpectCompromisedCredential(
              "Example App",
              "https://play.google.com/store/apps/details?id=com.example.app",
              "https://example.com/.well-known/change-password", kUsername2,
              base::Days(3), "3 days ago",
              {api::passwords_private::COMPROMISE_TYPE_PHISHED}),
          ExpectCompromisedCredential(
              "app.example.com",
              "https://play.google.com/store/apps/details?id=com.example.app",
              absl::nullopt, kUsername1, base::Days(4), "4 days ago",
              {api::passwords_private::COMPROMISE_TYPE_PHISHED}),
          ExpectCompromisedCredential(
              "example.com", "https://example.com/",
              "https://example.com/.well-known/change-password", kUsername1,
              base::Minutes(5), "5 minutes ago",
              {api::passwords_private::COMPROMISE_TYPE_LEAKED})));
}

// Test that a change to compromised credential notifies observers.
TEST_F(PasswordCheckDelegateTest, OnGetInsecureCredentials) {
  const char* const kEventName =
      api::passwords_private::OnInsecureCredentialsChanged::kEventName;

  // Verify that the event was not fired during construction.
  EXPECT_FALSE(base::Contains(event_router_observer().events(), kEventName));

  // Verify that the event gets fired once the compromised credential provider
  // is initialized.
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_INSECURE_CREDENTIALS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
  event_router_observer().ClearEvents();

  // Verify that a subsequent updating the form with a password issue results in
  // the expected event.
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  store().AddLogin(form);
  RunUntilIdle();
  AddIssueToForm(&form, InsecureType::kLeaked);
  store().UpdateLogin(form);
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_INSECURE_CREDENTIALS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
}

// Test that muting a insecure password succeeds.
TEST_F(PasswordCheckDelegateTest, MuteInsecureCredentialSuccess) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  EXPECT_TRUE(delegate().MuteInsecureCredential(credential));
  RunUntilIdle();
  EXPECT_TRUE(store()
                  .stored_passwords()
                  .at(kExampleCom)
                  .back()
                  .password_issues.at(InsecureType::kLeaked)
                  .is_muted.value());

  // Expect another mute of the same credential to fail.
  EXPECT_FALSE(delegate().MuteInsecureCredential(credential));
}

// Test that muting a insecure password fails if the underlying insecure
// credential no longer exists.
TEST_F(PasswordCheckDelegateTest, MuteInsecureCredentialStaleData) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  store().RemoveLogin(form);
  RunUntilIdle();

  EXPECT_FALSE(delegate().MuteInsecureCredential(credential));
}

// Test that muting a insecure password fails if the ids don't match.
TEST_F(PasswordCheckDelegateTest, MuteInsecureCredentialIdMismatch) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  EXPECT_EQ(0, credential.id);
  credential.id = 1;

  EXPECT_FALSE(delegate().MuteInsecureCredential(credential));
}

// Test that unmuting a muted insecure password succeeds.
TEST_F(PasswordCheckDelegateTest, UnmuteInsecureCredentialSuccess) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked, base::TimeDelta(), true);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  EXPECT_TRUE(delegate().UnmuteInsecureCredential(credential));
  RunUntilIdle();
  EXPECT_FALSE(store()
                   .stored_passwords()
                   .at(kExampleCom)
                   .back()
                   .password_issues.at(InsecureType::kLeaked)
                   .is_muted.value());

  // Expect another unmute of the same credential to fail.
  EXPECT_FALSE(delegate().UnmuteInsecureCredential(credential));
}

// Test that unmuting a insecure password fails if the underlying insecure
// credential no longer exists.
TEST_F(PasswordCheckDelegateTest, UnmuteInsecureCredentialStaleData) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked, base::TimeDelta(), true);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  store().RemoveLogin(form);
  RunUntilIdle();

  EXPECT_FALSE(delegate().UnmuteInsecureCredential(credential));
}

// Test that unmuting a insecure password fails if the ids don't match.
TEST_F(PasswordCheckDelegateTest, UnmuteInsecureCredentialIdMismatch) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked, base::TimeDelta(), true);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  EXPECT_EQ(0, credential.id);
  credential.id = 1;

  EXPECT_FALSE(delegate().UnmuteInsecureCredential(credential));
}

TEST_F(PasswordCheckDelegateTest, RecordChangePasswordFlowStarted) {
  // Create an insecure credential.
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  ASSERT_EQ(base::UTF16ToASCII(kUsername1), credential.username);

  EXPECT_CALL(
      password_change_success_tracker(),
      OnManualChangePasswordFlowStarted(
          GURL(*credential.change_password_url), credential.username,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings));

  delegate().RecordChangePasswordFlowStarted(credential);
}

TEST_F(PasswordCheckDelegateTest,
       RecordChangePasswordFlowStartedForAppWithWebRealm) {
  // Create an insecure credential.
  PasswordForm form = MakeSavedAndroidPassword(kExampleApp, kUsername2,
                                               "Example App", kExampleCom);
  AddIssueToForm(&form, InsecureType::kLeaked);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  ASSERT_EQ(base::UTF16ToASCII(kUsername2), credential.username);

  EXPECT_CALL(
      password_change_success_tracker(),
      OnManualChangePasswordFlowStarted(
          GURL(*credential.change_password_url), credential.username,
          PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings));

  delegate().RecordChangePasswordFlowStarted(credential);
}

TEST_F(PasswordCheckDelegateTest,
       RecordChangePasswordFlowStartedForAppWithoutWebRealm) {
  // Create an insecure credential.
  PasswordForm form = MakeSavedAndroidPassword(kExampleApp, kUsername1, "", "");
  AddIssueToForm(&form, InsecureType::kLeaked);
  store().AddLogin(form);
  RunUntilIdle();

  PasswordUiEntry credential =
      std::move(delegate().GetInsecureCredentials().at(0));
  ASSERT_EQ(base::UTF16ToASCII(kUsername1), credential.username);

  // Since no password change link exists, we expect no call to the tracker.
  EXPECT_CALL(password_change_success_tracker(),
              OnManualChangePasswordFlowStarted)
      .Times(0);

  delegate().RecordChangePasswordFlowStarted(credential);
}

// Tests that we don't create an entry in the database if there is no matching
// saved password.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundDoesNotCreateCredential) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  store().AddLogin(form);
  RunUntilIdle();
  delegate().StartPasswordCheck();
  store().RemoveLogin(form);
  RunUntilIdle();
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(kUsername1, kPassword1), IsLeaked(true));
  RunUntilIdle();

  EXPECT_TRUE(store().stored_passwords().at(kExampleCom).empty());
}

// Test that we don't create an entry in the password store if IsLeaked is
// false.
TEST_F(PasswordCheckDelegateTest, NoLeakedFound) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  store().AddLogin(form);
  RunUntilIdle();

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(kUsername1, kPassword1), IsLeaked(false));
  RunUntilIdle();

  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(kExampleCom, ElementsAre(form))));
}

// Test that a found leak creates a compromised credential in the password
// store.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundCreatesCredential) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  store().AddLogin(form);
  RunUntilIdle();

  delegate().StartPasswordCheck();
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(kUsername1, kPassword1), IsLeaked(true));
  RunUntilIdle();

  form.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  EXPECT_THAT(store().stored_passwords(),
              ElementsAre(Pair(kExampleCom, ElementsAre(form))));
}

// Test that a found leak creates a compromised credential in the password
// store for each combination of the same canonicalized username and password.
TEST_F(PasswordCheckDelegateTest, OnLeakFoundCreatesMultipleCredential) {
  const std::u16string kUsername2Upper = base::ToUpperASCII(kUsername2);
  const std::u16string kUsername2Email =
      base::StrCat({kUsername2, u"@email.com"});
  PasswordForm form_com_username1 =
      MakeSavedPassword(kExampleCom, kUsername1, kPassword1);
  store().AddLogin(form_com_username1);

  PasswordForm form_org_username1 =
      MakeSavedPassword(kExampleOrg, kUsername1, kPassword1);
  store().AddLogin(form_org_username1);

  PasswordForm form_com_username2 =
      MakeSavedPassword(kExampleCom, kUsername2Upper, kPassword2);
  store().AddLogin(form_com_username2);

  PasswordForm form_org_username2 =
      MakeSavedPassword(kExampleOrg, kUsername2Email, kPassword2);
  store().AddLogin(form_org_username2);

  RunUntilIdle();

  identity_test_env().MakeAccountAvailable(kTestEmail);
  delegate().StartPasswordCheck();
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(kUsername1, kPassword1), IsLeaked(true));
  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(kUsername2Email, kPassword2), IsLeaked(true));
  RunUntilIdle();

  form_com_username1.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  form_org_username1.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  form_com_username2.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  form_org_username2.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  EXPECT_THAT(store().stored_passwords(),
              UnorderedElementsAre(
                  Pair(kExampleCom, UnorderedElementsAre(form_com_username1,
                                                         form_com_username2)),
                  Pair(kExampleOrg, UnorderedElementsAre(form_org_username1,
                                                         form_org_username2))));
}

// Verifies that the case where the user has no saved passwords is reported
// correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusNoPasswords) {
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_NO_PASSWORDS,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the check is idle is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusIdle) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_IDLE,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the user is signed out is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusSignedOut) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_SIGNED_OUT,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the check is running is reported correctly and
// the progress indicator matches expectations.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusRunning) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING, status.state);
  EXPECT_EQ(0, *status.already_processed);
  EXPECT_EQ(1, *status.remaining_in_queue);

  // Make sure that even though the store is emptied after starting a check we
  // don't report a negative number for already processed.
  store().RemoveLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  status = delegate().GetPasswordCheckStatus();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING, status.state);
  EXPECT_EQ(0, *status.already_processed);
  EXPECT_EQ(1, *status.remaining_in_queue);
}

// Verifies that the total password count is reported accurately.
// Regression test for crbug.com/1432734.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusCount) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2, kPassword2));
  store().AddLogin(MakeSavedFederatedCredential(kExampleCom, kUsername3));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_EQ(*status.total_number_of_passwords, 2);
}

// Verifies that the case where the check is canceled is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusCanceled) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING,
            delegate().GetPasswordCheckStatus().state);

  delegate().StopPasswordCheck();
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_CANCELED,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the user is offline is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusOffline) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));

  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_OFFLINE,
            delegate().GetPasswordCheckStatus().state);
}

// Verifies that the case where the user hits another error (e.g. invalid
// credentials) is reported correctly.
TEST_F(PasswordCheckDelegateTest, GetPasswordCheckStatusOther) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  delegate().StartPasswordCheck();
  identity_test_env().WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_SIGNED_OUT,
            delegate().GetPasswordCheckStatus().state);
}

// Test that a change to the saved passwords notifies observers.
TEST_F(PasswordCheckDelegateTest,
       NotifyPasswordCheckStatusChangedAfterPasswordChange) {
  const char* const kEventName =
      api::passwords_private::OnPasswordCheckStatusChanged::kEventName;

  // Verify that the event was not fired during construction.
  EXPECT_FALSE(base::Contains(event_router_observer().events(), kEventName));

  // Verify that the event gets fired once the saved passwords provider is
  // initialized.
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
  event_router_observer().ClearEvents();

  // Verify that a subsequent call to AddLogin() results in the expected event.
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
}

// Test that a change to the bulk leak check notifies observers.
TEST_F(PasswordCheckDelegateTest,
       NotifyPasswordCheckStatusChangedAfterStateChange) {
  const char* const kEventName =
      api::passwords_private::OnPasswordCheckStatusChanged::kEventName;

  // Verify that the event was not fired during construction.
  EXPECT_FALSE(base::Contains(event_router_observer().events(), kEventName));

  // Verify that the event gets fired once the saved passwords provider is
  // initialized.
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
  event_router_observer().ClearEvents();

  // Verify that a subsequent call to StartPasswordCheck() results in the
  // expected event.
  delegate().StartPasswordCheck();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_router_observer().events().at(kEventName)->histogram_value);
}

// Checks that the default kLastTimePasswordCheckCompleted pref value is
// treated as no completed run yet.
TEST_F(PasswordCheckDelegateTest, LastTimePasswordCheckCompletedNotSet) {
  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_FALSE(status.elapsed_time_since_last_check);
}

// Checks that a non-default kLastTimePasswordCheckCompleted pref value is
// treated as a completed run, and formatted accordingly.
TEST_F(PasswordCheckDelegateTest, LastTimePasswordCheckCompletedIsSet) {
  profile().GetPrefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(5)).ToDoubleT());

  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_THAT(status.elapsed_time_since_last_check,
              Eq(std::string("5 minutes ago")));
}

// Checks that a transition into the idle state after starting a check results
// in resetting the kLastTimePasswordCheckCompleted pref to the current time.
TEST_F(PasswordCheckDelegateTest, LastTimePasswordCheckCompletedReset) {
  delegate().StartPasswordCheck();
  RunUntilIdle();

  service()->set_state_and_notify(BulkLeakCheckService::State::kIdle);
  PasswordCheckStatus status = delegate().GetPasswordCheckStatus();
  EXPECT_THAT(status.elapsed_time_since_last_check,
              Eq(std::string("Just now")));
}

// Checks that processing a credential by the leak check updates the progress
// correctly and raises the expected event.
TEST_F(PasswordCheckDelegateTest, OnCredentialDoneUpdatesProgress) {
  const char* const kEventName =
      api::passwords_private::OnPasswordCheckStatusChanged::kEventName;

  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2, kPassword2));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername2, kPassword2));
  RunUntilIdle();

  const auto event_iter = event_router_observer().events().find(kEventName);
  delegate().StartPasswordCheck();
  EXPECT_EQ(events::PASSWORDS_PRIVATE_ON_PASSWORD_CHECK_STATUS_CHANGED,
            event_iter->second->histogram_value);
  auto status = PasswordCheckStatus::FromValueDeprecated(
      event_iter->second->event_args.front());
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING,
            status->state);
  EXPECT_EQ(0, *status->already_processed);
  EXPECT_EQ(4, *status->remaining_in_queue);

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(kUsername1, kPassword1), IsLeaked(false));

  status = PasswordCheckStatus::FromValueDeprecated(
      event_iter->second->event_args.front());
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING,
            status->state);
  EXPECT_EQ(2, *status->already_processed);
  EXPECT_EQ(2, *status->remaining_in_queue);

  static_cast<BulkLeakCheckDelegateInterface*>(service())->OnFinishedCredential(
      LeakCheckCredential(kUsername2, kPassword2), IsLeaked(false));

  status = PasswordCheckStatus::FromValueDeprecated(
      event_iter->second->event_args.front());
  EXPECT_EQ(api::passwords_private::PASSWORD_CHECK_STATE_RUNNING,
            status->state);
  EXPECT_EQ(4, *status->already_processed);
  EXPECT_EQ(0, *status->remaining_in_queue);
}

// Tests that StopPasswordCheck() invokes pending callbacks before
// initialization finishes.
TEST_F(PasswordCheckDelegateTest,
       StopPasswordCheckRespondsCancelsBeforeInitialization) {
  MockStartPasswordCheckCallback callback1;
  MockStartPasswordCheckCallback callback2;
  delegate().StartPasswordCheck(callback1.Get());
  delegate().StartPasswordCheck(callback2.Get());

  EXPECT_CALL(callback1, Run(BulkLeakCheckService::State::kIdle));
  EXPECT_CALL(callback2, Run(BulkLeakCheckService::State::kIdle));
  delegate().StopPasswordCheck();

  Mock::VerifyAndClearExpectations(&callback1);
  Mock::VerifyAndClearExpectations(&callback2);
  RunUntilIdle();
}

// Tests that pending callbacks get invoked once initialization finishes.
TEST_F(PasswordCheckDelegateTest,
       StartPasswordCheckRunsCallbacksAfterInitialization) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  MockStartPasswordCheckCallback callback1;
  MockStartPasswordCheckCallback callback2;
  EXPECT_CALL(callback1, Run(BulkLeakCheckService::State::kRunning));
  EXPECT_CALL(callback2, Run(BulkLeakCheckService::State::kRunning));

  // Use a local delegate instead of |delegate()| so that the Password Store can
  // be set-up prior to constructing the object.
  password_manager::MockAffiliationService affiliation_service;
  SavedPasswordsPresenter new_presenter(&affiliation_service, &store(),
                                        /*account_store=*/nullptr);
  PasswordCheckDelegate delegate = CreateDelegate(&new_presenter);
  new_presenter.Init();
  delegate.StartPasswordCheck(callback1.Get());
  delegate.StartPasswordCheck(callback2.Get());
  RunUntilIdle();
}

TEST_F(PasswordCheckDelegateTest, WellKnownChangePasswordUrl) {
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form, InsecureType::kLeaked);
  store().AddLogin(form);

  RunUntilIdle();
  GURL change_password_url(
      *delegate().GetInsecureCredentials().at(0).change_password_url);
  EXPECT_EQ(change_password_url.path(),
            password_manager::kWellKnownChangePasswordPath);
}

TEST_F(PasswordCheckDelegateTest, WellKnownChangePasswordUrl_androidrealm) {
  PasswordForm form1 =
      MakeSavedAndroidPassword(kExampleApp, kUsername1, "", "");
  AddIssueToForm(&form1, InsecureType::kLeaked);
  store().AddLogin(form1);

  PasswordForm form2 = MakeSavedAndroidPassword(kExampleApp, kUsername2,
                                                "Example App", kExampleCom);
  form2.password_issues = form1.password_issues;
  store().AddLogin(form2);

  RunUntilIdle();

  EXPECT_FALSE(delegate().GetInsecureCredentials().at(0).change_password_url);
  EXPECT_EQ(GURL(*delegate().GetInsecureCredentials().at(1).change_password_url)
                .path(),
            password_manager::kWellKnownChangePasswordPath);
}

// Verify that GetCredentialsWithReusedPassword() correctly represents reused
// credentials.
TEST_F(PasswordCheckDelegateTest,
       GetCredentialsWithReusedPasswordFillsFieldsCorrectly) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kPasswordManagerRedesign);

  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2, kWeakPassword2));
  store().AddLogin(MakeSavedAndroidPassword(
      kExampleApp, kUsername2, "Example App", kExampleCom, kWeakPassword1));
  store().AddLogin(MakeSavedAndroidPassword(
      kExampleApp, kUsername1, "Example App", kExampleCom, kWeakPassword2));
  RunUntilIdle();
  delegate().StartPasswordCheck();
  RunUntilIdle();

  auto credential_with_the_same_password =
      delegate().GetCredentialsWithReusedPassword();
  EXPECT_EQ(2u, credential_with_the_same_password.size());

  EXPECT_THAT(
      credential_with_the_same_password,
      UnorderedElementsAre(
          Field(&api::passwords_private::PasswordUiEntryList::entries,
                UnorderedElementsAre(
                    ExpectCredential(
                        "example.com", "https://example.com/",
                        "https://example.com/.well-known/change-password",
                        kUsername2),
                    ExpectCredential(
                        "Example App",
                        "https://play.google.com/store/apps/"
                        "details?id=com.example.app",
                        "https://example.com/.well-known/change-password",
                        kUsername1))),
          Field(&api::passwords_private::PasswordUiEntryList::entries,
                UnorderedElementsAre(
                    ExpectCredential(
                        "example.com", "https://example.com/",
                        "https://example.com/.well-known/change-password",
                        kUsername1),
                    ExpectCredential(
                        "Example App",
                        "https://play.google.com/store/apps/"
                        "details?id=com.example.app",
                        "https://example.com/.well-known/change-password",
                        kUsername2)))));
}

TEST_F(PasswordCheckDelegateTest,
       GetCredentialsWithReusedPasswordAvoidsSingleReuse) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::kPasswordManagerRedesign);

  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  store().AddLogin(MakeSavedPassword(kExampleApp, kUsername2, kWeakPassword1));
  RunUntilIdle();
  delegate().StartPasswordCheck();
  RunUntilIdle();

  EXPECT_EQ(1u, delegate().GetCredentialsWithReusedPassword().size());

  store().RemoveLogin(
      MakeSavedPassword(kExampleCom, kUsername1, kWeakPassword1));
  RunUntilIdle();

  EXPECT_THAT(delegate().GetCredentialsWithReusedPassword(), IsEmpty());
}

}  // namespace extensions
