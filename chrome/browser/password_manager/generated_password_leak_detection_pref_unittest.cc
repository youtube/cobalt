// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/generated_password_leak_detection_pref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestProfileName[] = "test@test.com";

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<TestingProfile> BuildTestProfile(
    network::TestURLLoaderFactory& url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetProfileName(kTestProfileName);
  profile_builder.AddTestingFactory(
      ChromeSigninClientFactory::GetInstance(),
      base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                          &url_loader_factory));
  return IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment(profile_builder);
}

}  // namespace

namespace settings_api = extensions::api::settings_private;
namespace settings_private = extensions::settings_private;

// The test parameter controls whether the user is signed in.
class GeneratedPasswordLeakDetectionPrefTest
    : public testing::TestWithParam<bool> {
 public:
  GeneratedPasswordLeakDetectionPrefTest() {
    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
  }

  // Initializes the identity environment based on the |signed_in| parameter.
  // For signed in users adds primary account, otherwise enables the feature for
  // unauthenticated leak detection.
  void InitializeUserAccount(bool signed_in) {
    if (signed_in) {
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestProfileName, signin::ConsentLevel::kSignin);
    } else {
      feature_list_.InitAndEnableFeature(
          password_manager::features::kLeakDetectionUnauthenticated);
    }
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_.identity_test_env();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }

  TestingProfile* profile() { return profile_.get(); }

  syncer::TestSyncService* sync_service() { return sync_service_; }

  content::BrowserTaskEnvironment task_environment_;

 private:
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_ =
      BuildTestProfile(test_url_loader_factory_);
  raw_ptr<syncer::TestSyncService> sync_service_ =
      static_cast<syncer::TestSyncService*>(
          SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              profile(),
              base::BindRepeating(&BuildTestSyncService)));
  IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor_{
      profile_.get()};
};

TEST_P(GeneratedPasswordLeakDetectionPrefTest, NotifyPrefUpdates) {
  // This test initializes non-default identity described below, so there is no
  // point of running it twice.
  if (GetParam() == false) {
    return;
  }

  // Check that when source information changes, the pref observer is fired.
  GeneratedPasswordLeakDetectionPref pref(profile());
  settings_private::TestGeneratedPrefObserver test_observer;
  pref.AddObserver(&test_observer);

  // Check that the observer fires for identity updates.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();

  // Create a sync consented account so revoking the refresh token also triggers
  // the preference updated observer.
  identity_test_env()->MakePrimaryAccountAvailable(kTestProfileName,
                                                   signin::ConsentLevel::kSync);
  EXPECT_EQ(test_observer.GetUpdatedPrefName(),
            kGeneratedPasswordLeakDetectionPref);

  test_observer.Reset();
  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();
  EXPECT_EQ(test_observer.GetUpdatedPrefName(),
            kGeneratedPasswordLeakDetectionPref);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Clearing the primary account does not make sense on ChromeOS.
  test_observer.Reset();
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_EQ(test_observer.GetUpdatedPrefName(),
            kGeneratedPasswordLeakDetectionPref);
#endif  // !defined (OS_CHROMEOS)

  // Check the observer fires for source preference updates.
  test_observer.Reset();
  prefs()->SetUserPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(),
            kGeneratedPasswordLeakDetectionPref);

  test_observer.Reset();
  prefs()->SetUserPref(prefs::kSafeBrowsingEnhanced,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(),
            kGeneratedPasswordLeakDetectionPref);

  test_observer.Reset();
  prefs()->SetUserPref(password_manager::prefs::kPasswordLeakDetectionEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_EQ(test_observer.GetUpdatedPrefName(),
            kGeneratedPasswordLeakDetectionPref);

  // // Check the observer fires for sync service updates.
  test_observer.Reset();
  sync_service()->FireStateChanged();
  EXPECT_EQ(test_observer.GetUpdatedPrefName(),
            kGeneratedPasswordLeakDetectionPref);
}

TEST_P(GeneratedPasswordLeakDetectionPrefTest, UpdatePreference) {
  InitializeUserAccount(/*signed_in=*/GetParam());

  // Check the generated pref both updates, and respects updates to, the
  // underlying preference.
  GeneratedPasswordLeakDetectionPref pref(profile());

  // Setup baseline profile preference & signin state.
  prefs()->SetDefaultPrefValue(
      password_manager::prefs::kPasswordLeakDetectionEnabled,
      base::Value(false));

  // Check setting the generated pref updates the underlying preference.
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(true).get()),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_TRUE(
      prefs()
          ->GetUserPref(password_manager::prefs::kPasswordLeakDetectionEnabled)
          ->GetBool());

  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(false).get()),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_FALSE(
      prefs()
          ->GetUserPref(password_manager::prefs::kPasswordLeakDetectionEnabled)
          ->GetBool());

  // Check that changing the underlying preference correctly updates the
  // generated pref.
  prefs()->SetUserPref(password_manager::prefs::kPasswordLeakDetectionEnabled,
                       std::make_unique<base::Value>(true));
  EXPECT_TRUE(pref.GetPrefObject().value->GetBool());

  prefs()->SetUserPref(password_manager::prefs::kPasswordLeakDetectionEnabled,
                       std::make_unique<base::Value>(false));
  EXPECT_FALSE(pref.GetPrefObject().value->GetBool());

  // Confirm that a type mismatch is reported as such.
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(2).get()),
            extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH);
}

TEST_P(GeneratedPasswordLeakDetectionPrefTest, ProfileState) {
  InitializeUserAccount(/*signed_in=*/GetParam());

  GeneratedPasswordLeakDetectionPref pref(profile());
  prefs()->SetUserPref(password_manager::prefs::kPasswordLeakDetectionEnabled,
                       std::make_unique<base::Value>(true));

  // Check that when Safe Browsing is set to standard, both user control and the
  // pref are enabled.
  prefs()->SetUserPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kSafeBrowsingEnhanced,
                       std::make_unique<base::Value>(false));
  EXPECT_TRUE(pref.GetPrefObject().value->GetBool());
  EXPECT_FALSE(*pref.GetPrefObject().user_control_disabled);

  // Set Safe Browsing to disabled, check that user control is disabled and pref
  // cannot be modified, but the pref value remains enabled.
  prefs()->SetUserPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(false));
  EXPECT_TRUE(pref.GetPrefObject().value->GetBool());
  EXPECT_TRUE(*pref.GetPrefObject().user_control_disabled);
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(true).get()),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
}

TEST_P(GeneratedPasswordLeakDetectionPrefTest, ManagementState) {
  InitializeUserAccount(/*signed_in=*/GetParam());

  // Check that the management state of the underlying preference is applied
  // to the generated preference.
  GeneratedPasswordLeakDetectionPref pref(profile());
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::ENFORCEMENT_NONE);
  EXPECT_EQ(pref.GetPrefObject().controlled_by,
            settings_api::ControlledBy::CONTROLLED_BY_NONE);

  prefs()->SetRecommendedPref(
      password_manager::prefs::kPasswordLeakDetectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::ENFORCEMENT_RECOMMENDED);
  EXPECT_EQ(pref.GetPrefObject().recommended_value->GetBool(), true);

  prefs()->SetManagedPref(
      password_manager::prefs::kPasswordLeakDetectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_EQ(pref.GetPrefObject().enforcement,
            settings_api::Enforcement::ENFORCEMENT_ENFORCED);
  EXPECT_EQ(pref.GetPrefObject().controlled_by,
            settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY);

  // Check that the preference cannot be changed when the backing preference is
  // managed, but the preference could otherwise be changed.
  prefs()->SetUserPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kSafeBrowsingEnhanced,
                       std::make_unique<base::Value>(false));
  EXPECT_EQ(pref.SetPref(std::make_unique<base::Value>(true).get()),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
}

INSTANTIATE_TEST_SUITE_P(,
                         GeneratedPasswordLeakDetectionPrefTest,
                         testing::Bool());
