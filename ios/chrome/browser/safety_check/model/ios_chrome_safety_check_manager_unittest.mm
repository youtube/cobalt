// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"

#import <vector>

#import "base/memory/scoped_refptr.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_utils.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class IOSChromeSafetyCheckManagerTest : public PlatformTest {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefRegistrySimple* registry = pref_service_->registry();

    registry->RegisterBooleanPref(prefs::kSafeBrowsingEnabled, false);
    registry->RegisterBooleanPref(prefs::kSafeBrowsingEnhanced, false);

    local_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefRegistrySimple* local_registry = local_pref_service_->registry();

    local_registry->RegisterTimePref(prefs::kIosSafetyCheckManagerLastRunTime,
                                     base::Time(), PrefRegistry::LOSSY_PREF);
    local_registry->RegisterStringPref(
        prefs::kIosSafetyCheckManagerPasswordCheckResult,
        NameForSafetyCheckState(PasswordSafetyCheckState::kDefault),
        PrefRegistry::LOSSY_PREF);
    local_registry->RegisterStringPref(
        prefs::kIosSafetyCheckManagerUpdateCheckResult,
        NameForSafetyCheckState(UpdateChromeSafetyCheckState::kDefault),
        PrefRegistry::LOSSY_PREF);
    local_registry->RegisterStringPref(
        prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
        NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kDefault),
        PrefRegistry::LOSSY_PREF);
    local_registry->RegisterIntegerPref(
        prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness,
        -1);

    TestChromeBrowserState::Builder builder;

    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    browser_state_ = builder.Build();
    TestingApplicationContext::GetGlobal()->SetLocalState(
        local_pref_service_.get());

    password_check_manager_ =
        IOSChromePasswordCheckManagerFactory::GetForBrowserState(
            browser_state_.get());

    safety_check_manager_ = std::make_unique<IOSChromeSafetyCheckManager>(
        pref_service_.get(), local_pref_service_.get(), password_check_manager_,
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    safety_check_manager_->StopSafetyCheck();
    safety_check_manager_->Shutdown();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<IOSChromeSafetyCheckManager> safety_check_manager_;
  scoped_refptr<IOSChromePasswordCheckManager> password_check_manager_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_pref_service_;
};

std::vector<password_manager::CredentialUIEntry>
CreateCredentialsListWithNoInsecurePasswords() {
  password_manager::PasswordForm password_form;
  password_form.url = GURL("http://accounts.google.com/a/LoginAuth");
  password_form.username_value = u"test@testmail.com";
  password_form.password_value = u"test1";

  return {password_manager::CredentialUIEntry(password_form)};
}

// Returns app upgrade details for an up-to-date application.
UpgradeRecommendedDetails UpdatedAppDetails() {
  UpgradeRecommendedDetails details;

  details.is_up_to_date = true;

  // Within Omaha, when the app is up-to-date, `next_version` and `upgrade_url`
  // are empty.
  details.next_version = "";
  details.upgrade_url = GURL();

  return details;
}

// Returns app upgrade details for an outdated application.
UpgradeRecommendedDetails OutdatedAppDetails() {
  UpgradeRecommendedDetails details;

  details.is_up_to_date = false;
  details.next_version = "9999.9999.9999.9999";
  details.upgrade_url = GURL("http://orgForName.org");

  return details;
}

}  // namespace

// Tests the the last run time of the Safety Check is unset if the Safety Check
// hasn't been run, yet.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ReturnsZeroSafetyCheckRunTimeIfNeverRun) {
  EXPECT_EQ(safety_check_manager_->GetLastSafetyCheckRunTime(), base::Time());
}

// Tests the the last run time of the Safety Check is correctly returned if the
// Safety Check has previously run.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ReturnsLastSafetyCheckRunTimeIfPreviouslyRun) {
  base::Time now = base::Time::Now();

  safety_check_manager_->StartSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetLastSafetyCheckRunTime(), now);
}

// Tests the the last run time of the Safety Check is correctly written to
// Prefs.
TEST_F(IOSChromeSafetyCheckManagerTest, LogsSafetyCheckRunTime) {
  base::Time now = base::Time::Now();

  safety_check_manager_->StartSafetyCheck();

  EXPECT_EQ(
      local_pref_service_->GetTime(prefs::kIosSafetyCheckManagerLastRunTime),
      now);
}

// Tests the Safe Browsing Check state is `kSafe` when Safe Browsing is enabled,
// but not managed.
TEST_F(IOSChromeSafetyCheckManagerTest, SafeBrowsingEnabledReturnsSafeState) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kSafe);
}

// Tests the Safe Browsing Check state is `kUnsafe` when Safe Browsing is
// disabled, and not managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingDisabledReturnsUnsafeState) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kUnsafe);
}

// Tests the Safe Browsing Check state is `kManaged` when Safe Browsing is
// enabled, and managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingManagedAndEnabledReturnsManagedState) {
  pref_service_->SetManagedPref(prefs::kSafeBrowsingEnabled, base::Value(true));

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kManaged);
}

// Tests the Safe Browsing Check state is `kManaged` when Safe Browsing is
// disabled, and managed.
TEST_F(IOSChromeSafetyCheckManagerTest,
       SafeBrowsingManagedAndDisabledReturnsManagedState) {
  pref_service_->SetManagedPref(prefs::kSafeBrowsingEnabled,
                                base::Value(false));

  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kManaged);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kRunning.
TEST_F(IOSChromeSafetyCheckManagerTest, ConvertsPasswordCheckStateRunning) {
  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kRunning, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kRunning);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kRunning, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kRunning);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kNoPasswords.
TEST_F(IOSChromeSafetyCheckManagerTest, ConvertsPasswordCheckStateNoPasswords) {
  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kNoPasswords, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDisabled);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kNoPasswords, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDisabled);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kSignedOut when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateSignedOutWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kSignedOut, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kSignedOut);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kSignedOut, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kSignedOut when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateSignedOutWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kSignedOut, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kSignedOut);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kSignedOut, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kSignedOut);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kOffline when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateOfflineWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOffline, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOffline, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kOffline when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateOfflineWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOffline, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOffline, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kQuotaLimit when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateQuotaLimitWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kQuotaLimit, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kQuotaLimit, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kQuotaLimit when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateQuotaLimitWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kQuotaLimit, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kQuotaLimit, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kOther when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateOtherWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOther, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOther, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kOther when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateOtherWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOther, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kOther, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kError);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kCanceled when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateCanceledWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kCanceled, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kCanceled, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kCanceled when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateCanceledWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kCanceled, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kCanceled, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kIdle when `kIOSPasswordCheckup` is disabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateIdleWithPasswordCheckupDisabled) {
  feature_list_.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, empty_credentials_list,
                PasswordSafetyCheckState::kRunning),
            PasswordSafetyCheckState::kSafe);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, populated_credentials_list,
                PasswordSafetyCheckState::kRunning),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);
}

// Tests `CalculatePasswordSafetyCheckState()` correctly converts
// PasswordCheckState::kIdle when `kIOSPasswordCheckup` is enabled.
TEST_F(IOSChromeSafetyCheckManagerTest,
       ConvertsPasswordCheckStateIdleWithPasswordCheckupEnabled) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kIOSPasswordCheckup);

  std::vector<password_manager::CredentialUIEntry> empty_credentials_list;
  std::vector<password_manager::CredentialUIEntry> populated_credentials_list =
      CreateCredentialsListWithNoInsecurePasswords();

  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, empty_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, populated_credentials_list,
                safety_check_manager_->GetPasswordCheckState()),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, empty_credentials_list,
                PasswordSafetyCheckState::kRunning),
            PasswordSafetyCheckState::kSafe);
  EXPECT_EQ(CalculatePasswordSafetyCheckState(
                PasswordCheckState::kIdle, populated_credentials_list,
                PasswordSafetyCheckState::kRunning),
            PasswordSafetyCheckState::kSafe);
}

// Tests an Omaha response that exceeds `kOmahaNetworkWaitTime` wait time is
// properly handled.
TEST_F(IOSChromeSafetyCheckManagerTest, HandlesExpiredOmahaResponse) {
  // Starting the Omaha check sets the Update Chrome check state to running.
  safety_check_manager_->StartOmahaCheckForTesting();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  // Even 1s before `kOmahaNetworkWaitTime` is met, the check state should still
  // be running.
  task_environment_.FastForwardBy(kOmahaNetworkWaitTime - base::Seconds(1));

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  // Once `kOmahaNetworkWaitTime` is met, the current Omaha request should be
  // considered an Omaha error.
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kOmahaError);
}

// Tests a valid, app-up-to-date Omaha response is properly handled.
TEST_F(IOSChromeSafetyCheckManagerTest, HandlesOmahaResponseAppIsUpToDate) {
  safety_check_manager_->StartOmahaCheckForTesting();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);

  safety_check_manager_->HandleOmahaResponseForTesting(UpdatedAppDetails());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kUpToDate);

  // Once `kOmahaNetworkWaitTime` elapses, nothing should happen, because the
  // response was received before `kOmahaNetworkWaitTime` was met.
  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kUpToDate);
}

// Tests a valid, app-outdated Omaha response is properly handled.
TEST_F(IOSChromeSafetyCheckManagerTest, HandlesOmahaResponseAppOutdated) {
  safety_check_manager_->StartOmahaCheckForTesting();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);

  safety_check_manager_->HandleOmahaResponseForTesting(OutdatedAppDetails());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kOutOfDate);

  // Once `kOmahaNetworkWaitTime` elapses, nothing should happen, because the
  // response was received before `kOmahaNetworkWaitTime` was met.
  task_environment_.FastForwardBy(kOmahaNetworkWaitTime / 2);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kOutOfDate);
}

// Tests starting the Safety Check updates all check states.
TEST_F(IOSChromeSafetyCheckManagerTest,
       StartingSafetyCheckUpdatesAllCheckStates) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  safety_check_manager_->StartSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  // The Password check state is expected to be `kDefault` here because it's not
  // directly controlled by the Safety Check Manager. The Safety Check Manager
  // observes Password check changes, i.e. when the Password check begins
  // running in the Password Check Manager, the Safety Check
  // Manager will be notified via an observer call.
  //
  // At that point, the Safety Check Manager will have the new Password check
  // state `kRunning`, but it won't necessarily be `kRunning` here.
  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);

  // The Safe Browsing check is synchronous, so it will immediately return its
  // value, i.e. no `kRunning` state.
  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kSafe);

  EXPECT_EQ(safety_check_manager_->GetRunningCheckStateForTesting(),
            RunningSafetyCheckState::kRunning);
}

// Tests stopping a currently running Safety Check reverts all check
// states to their previous value.
TEST_F(IOSChromeSafetyCheckManagerTest,
       StoppingRunningSafetyCheckRevertsAllCheckStates) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  safety_check_manager_->StartSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);
  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kSafe);
  EXPECT_EQ(safety_check_manager_->GetRunningCheckStateForTesting(),
            RunningSafetyCheckState::kRunning);

  safety_check_manager_->StopSafetyCheck();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kDefault);
  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDisabled);
  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kSafe);
  EXPECT_EQ(safety_check_manager_->GetRunningCheckStateForTesting(),
            RunningSafetyCheckState::kDefault);
}

// Tests cancelling a currently running Safety Check check ignores an
// incoming Omaha response.
TEST_F(IOSChromeSafetyCheckManagerTest,
       StoppingRunningUpdateChromeCheckIgnoresOmahaResponse) {
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kDefault);

  safety_check_manager_->StartSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  safety_check_manager_->StopSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kDefault);

  // NOTE: Normally this call would change the Update Chrome check state to
  // `kUpToDate`. However, this call should be ignored because the Safety
  // Check was cancelled, reverting the check state `kDefault`, and ignoring the
  // future update below.
  safety_check_manager_->HandleOmahaResponseForTesting(UpdatedAppDetails());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kDefault);
}

// Tests cancelling a currently running Safety Check check correctly ignores an
// incoming Omaha error.
TEST_F(IOSChromeSafetyCheckManagerTest,
       StoppingRunningUpdateChromeCheckIgnoresOmahaError) {
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kDefault);

  safety_check_manager_->StartSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kRunning);

  safety_check_manager_->StopSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kDefault);

  // NOTE: Normally this call would change the Update Chrome check state to
  // `kOmahaError`. However, this call should be ignored because the Safety
  // Check was cancelled, reverting the check state `kDefault`, and ignoring the
  // future error below.
  safety_check_manager_->HandleOmahaResponseForTesting(OutdatedAppDetails());

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kDefault);
}

// Tests cancelling a currently running Safety Check check ignores an
// incoming Password Check change.
TEST_F(IOSChromeSafetyCheckManagerTest,
       StoppingRunningPasswordCheckIgnoresPasswordCheckChange) {
  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);

  safety_check_manager_->StartSafetyCheck();

  safety_check_manager_->SetPasswordCheckStateForTesting(
      PasswordSafetyCheckState::kRunning);

  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kRunning);

  safety_check_manager_->StopSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);

  // NOTE: Normally this call would change the Password check state to
  // `kError` due to the quota limit being reached. However, this call should be
  // ignored because the Password check was cancelled, reverting the check state
  // `kDefault`, and ignoring the future update below.
  safety_check_manager_->PasswordCheckStatusChangedForTesting(
      PasswordCheckState::kQuotaLimit);

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);
}

// Tests cancelling a currently running Safety Check check correctly ignores an
// incoming insecure credentials change.
TEST_F(IOSChromeSafetyCheckManagerTest,
       StoppingRunningPasswordCheckIgnoresInsecureCredentialsChange) {
  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);

  safety_check_manager_->StartSafetyCheck();

  safety_check_manager_->SetPasswordCheckStateForTesting(
      PasswordSafetyCheckState::kRunning);

  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kRunning);

  safety_check_manager_->StopSafetyCheck();

  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);

  // NOTE: Normally this call would change the Password check state. However,
  // this call should be ignored because the Password check was cancelled,
  // reverting the check state `kDefault`, and ignoring the future update
  // below.
  safety_check_manager_->InsecureCredentialsChangedForTesting();

  task_environment_.RunUntilIdle();

  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);
}

// Tests correctly generating a string representation of
// `UpdateChromeSafetyCheckState`.
TEST_F(IOSChromeSafetyCheckManagerTest, CreatesUpdateChromeSafetyCheckName) {
  EXPECT_EQ(NameForSafetyCheckState(UpdateChromeSafetyCheckState::kDefault),
            "UpdateChromeSafetyCheckState::kDefault");

  EXPECT_EQ(NameForSafetyCheckState(UpdateChromeSafetyCheckState::kUpToDate),
            "UpdateChromeSafetyCheckState::kUpToDate");

  EXPECT_EQ(NameForSafetyCheckState(UpdateChromeSafetyCheckState::kOutOfDate),
            "UpdateChromeSafetyCheckState::kOutOfDate");

  EXPECT_EQ(NameForSafetyCheckState(UpdateChromeSafetyCheckState::kManaged),
            "UpdateChromeSafetyCheckState::kManaged");

  EXPECT_EQ(NameForSafetyCheckState(UpdateChromeSafetyCheckState::kRunning),
            "UpdateChromeSafetyCheckState::kRunning");

  EXPECT_EQ(NameForSafetyCheckState(UpdateChromeSafetyCheckState::kOmahaError),
            "UpdateChromeSafetyCheckState::kOmahaError");

  EXPECT_EQ(NameForSafetyCheckState(UpdateChromeSafetyCheckState::kNetError),
            "UpdateChromeSafetyCheckState::kNetError");

  EXPECT_EQ(NameForSafetyCheckState(UpdateChromeSafetyCheckState::kChannel),
            "UpdateChromeSafetyCheckState::kChannel");
}

// Tests correctly finding the corresponding `UpdateChromeSafetyCheckState`
// given its string representation.
TEST_F(IOSChromeSafetyCheckManagerTest, FindsUpdateChromeSafetyCheckFromName) {
  EXPECT_EQ(UpdateChromeSafetyCheckStateForName(
                "UpdateChromeSafetyCheckState::kDefault")
                .value(),
            UpdateChromeSafetyCheckState::kDefault);

  EXPECT_EQ(UpdateChromeSafetyCheckStateForName(
                "UpdateChromeSafetyCheckState::kUpToDate")
                .value(),
            UpdateChromeSafetyCheckState::kUpToDate);

  EXPECT_EQ(UpdateChromeSafetyCheckStateForName(
                "UpdateChromeSafetyCheckState::kOutOfDate")
                .value(),
            UpdateChromeSafetyCheckState::kOutOfDate);

  EXPECT_EQ(UpdateChromeSafetyCheckStateForName(
                "UpdateChromeSafetyCheckState::kManaged")
                .value(),
            UpdateChromeSafetyCheckState::kManaged);

  EXPECT_EQ(UpdateChromeSafetyCheckStateForName(
                "UpdateChromeSafetyCheckState::kRunning")
                .value(),
            UpdateChromeSafetyCheckState::kRunning);

  EXPECT_EQ(UpdateChromeSafetyCheckStateForName(
                "UpdateChromeSafetyCheckState::kOmahaError")
                .value(),
            UpdateChromeSafetyCheckState::kOmahaError);

  EXPECT_EQ(UpdateChromeSafetyCheckStateForName(
                "UpdateChromeSafetyCheckState::kNetError")
                .value(),
            UpdateChromeSafetyCheckState::kNetError);

  EXPECT_EQ(UpdateChromeSafetyCheckStateForName(
                "UpdateChromeSafetyCheckState::kChannel")
                .value(),
            UpdateChromeSafetyCheckState::kChannel);

  // Invalid cases
  EXPECT_FALSE(UpdateChromeSafetyCheckStateForName(
                   "UpdateChromeSafetyCheckState::kFoobar")
                   .has_value());
}

// Tests correctly generating a string representation of
// `PasswordSafetyCheckState`.
TEST_F(IOSChromeSafetyCheckManagerTest, CreatesPasswordSafetyCheckName) {
  EXPECT_EQ(NameForSafetyCheckState(PasswordSafetyCheckState::kDefault),
            "PasswordSafetyCheckState::kDefault");

  EXPECT_EQ(NameForSafetyCheckState(PasswordSafetyCheckState::kSafe),
            "PasswordSafetyCheckState::kSafe");

  EXPECT_EQ(NameForSafetyCheckState(
                PasswordSafetyCheckState::kUnmutedCompromisedPasswords),
            "PasswordSafetyCheckState::kUnmutedCompromisedPasswords");

  EXPECT_EQ(NameForSafetyCheckState(PasswordSafetyCheckState::kReusedPasswords),
            "PasswordSafetyCheckState::kReusedPasswords");

  EXPECT_EQ(NameForSafetyCheckState(PasswordSafetyCheckState::kWeakPasswords),
            "PasswordSafetyCheckState::kWeakPasswords");

  EXPECT_EQ(
      NameForSafetyCheckState(PasswordSafetyCheckState::kDismissedWarnings),
      "PasswordSafetyCheckState::kDismissedWarnings");

  EXPECT_EQ(NameForSafetyCheckState(PasswordSafetyCheckState::kRunning),
            "PasswordSafetyCheckState::kRunning");

  EXPECT_EQ(NameForSafetyCheckState(PasswordSafetyCheckState::kDisabled),
            "PasswordSafetyCheckState::kDisabled");

  EXPECT_EQ(NameForSafetyCheckState(PasswordSafetyCheckState::kError),
            "PasswordSafetyCheckState::kError");

  EXPECT_EQ(NameForSafetyCheckState(PasswordSafetyCheckState::kSignedOut),
            "PasswordSafetyCheckState::kSignedOut");
}

// Tests correctly finding the corresponding `PasswordSafetyCheckState` given
// its string representation.
TEST_F(IOSChromeSafetyCheckManagerTest, FindsPasswordSafetyCheckFromName) {
  EXPECT_EQ(
      PasswordSafetyCheckStateForName("PasswordSafetyCheckState::kDefault")
          .value(),
      PasswordSafetyCheckState::kDefault);

  EXPECT_EQ(PasswordSafetyCheckStateForName("PasswordSafetyCheckState::kSafe")
                .value(),
            PasswordSafetyCheckState::kSafe);

  EXPECT_EQ(PasswordSafetyCheckStateForName(
                "PasswordSafetyCheckState::kUnmutedCompromisedPasswords")
                .value(),
            PasswordSafetyCheckState::kUnmutedCompromisedPasswords);

  EXPECT_EQ(PasswordSafetyCheckStateForName(
                "PasswordSafetyCheckState::kReusedPasswords")
                .value(),
            PasswordSafetyCheckState::kReusedPasswords);

  EXPECT_EQ(PasswordSafetyCheckStateForName(
                "PasswordSafetyCheckState::kWeakPasswords")
                .value(),
            PasswordSafetyCheckState::kWeakPasswords);

  EXPECT_EQ(PasswordSafetyCheckStateForName(
                "PasswordSafetyCheckState::kDismissedWarnings")
                .value(),
            PasswordSafetyCheckState::kDismissedWarnings);

  EXPECT_EQ(
      PasswordSafetyCheckStateForName("PasswordSafetyCheckState::kRunning")
          .value(),
      PasswordSafetyCheckState::kRunning);

  EXPECT_EQ(
      PasswordSafetyCheckStateForName("PasswordSafetyCheckState::kDisabled")
          .value(),
      PasswordSafetyCheckState::kDisabled);

  EXPECT_EQ(PasswordSafetyCheckStateForName("PasswordSafetyCheckState::kError")
                .value(),
            PasswordSafetyCheckState::kError);

  EXPECT_EQ(
      PasswordSafetyCheckStateForName("PasswordSafetyCheckState::kSignedOut")
          .value(),
      PasswordSafetyCheckState::kSignedOut);

  // Invalid cases
  EXPECT_FALSE(
      PasswordSafetyCheckStateForName("PasswordSafetyCheckState::kFoobar")
          .has_value());
}

// Tests correctly generating a string representation of
// `SafeBrowsingSafetyCheckState`.
TEST_F(IOSChromeSafetyCheckManagerTest, CreatesSafeBrowsingSafetyCheckName) {
  EXPECT_EQ(NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kDefault),
            "SafeBrowsingSafetyCheckState::kDefault");
  EXPECT_EQ(NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kManaged),
            "SafeBrowsingSafetyCheckState::kManaged");
  EXPECT_EQ(NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kRunning),
            "SafeBrowsingSafetyCheckState::kRunning");
  EXPECT_EQ(NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kSafe),
            "SafeBrowsingSafetyCheckState::kSafe");
  EXPECT_EQ(NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kUnsafe),
            "SafeBrowsingSafetyCheckState::kUnsafe");
}

// Tests correctly finding the corresponding `SafeBrowsingSafetyCheckState`
// given its string representation.
TEST_F(IOSChromeSafetyCheckManagerTest, FindsSafeBrowsingSafetyCheckFromName) {
  EXPECT_EQ(SafeBrowsingSafetyCheckStateForName(
                "SafeBrowsingSafetyCheckState::kDefault")
                .value(),
            SafeBrowsingSafetyCheckState::kDefault);

  EXPECT_EQ(SafeBrowsingSafetyCheckStateForName(
                "SafeBrowsingSafetyCheckState::kManaged")
                .value(),
            SafeBrowsingSafetyCheckState::kManaged);

  EXPECT_EQ(SafeBrowsingSafetyCheckStateForName(
                "SafeBrowsingSafetyCheckState::kRunning")
                .value(),
            SafeBrowsingSafetyCheckState::kRunning);

  EXPECT_EQ(
      SafeBrowsingSafetyCheckStateForName("SafeBrowsingSafetyCheckState::kSafe")
          .value(),
      SafeBrowsingSafetyCheckState::kSafe);

  EXPECT_EQ(SafeBrowsingSafetyCheckStateForName(
                "SafeBrowsingSafetyCheckState::kUnsafe")
                .value(),
            SafeBrowsingSafetyCheckState::kUnsafe);
}

// Tests `RestorePreviousSafetyCheckState()` correctly loads previous Safety
// Check states from Prefs.
TEST_F(IOSChromeSafetyCheckManagerTest, LoadsPreviousCheckStatesFromPrefs) {
  local_pref_service_->SetString(
      prefs::kIosSafetyCheckManagerPasswordCheckResult,
      NameForSafetyCheckState(PasswordSafetyCheckState::kError));
  local_pref_service_->SetString(
      prefs::kIosSafetyCheckManagerUpdateCheckResult,
      NameForSafetyCheckState(UpdateChromeSafetyCheckState::kOutOfDate));
  local_pref_service_->SetString(
      prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
      NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kSafe));

  safety_check_manager_->RestorePreviousSafetyCheckStateForTesting();

  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kError);
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kOutOfDate);
  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kSafe);
}

// Tests `RestorePreviousSafetyCheckState()` correctly loads previous Safety
// Check states from Prefs. and ignores running states.
TEST_F(IOSChromeSafetyCheckManagerTest,
       LoadsPreviousCheckStatesFromPrefsButIgnoresRunningStates) {
  local_pref_service_->SetString(
      prefs::kIosSafetyCheckManagerPasswordCheckResult,
      NameForSafetyCheckState(PasswordSafetyCheckState::kRunning));
  local_pref_service_->SetString(
      prefs::kIosSafetyCheckManagerUpdateCheckResult,
      NameForSafetyCheckState(UpdateChromeSafetyCheckState::kOutOfDate));
  local_pref_service_->SetString(
      prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
      NameForSafetyCheckState(SafeBrowsingSafetyCheckState::kRunning));

  safety_check_manager_->RestorePreviousSafetyCheckStateForTesting();

  EXPECT_EQ(safety_check_manager_->GetPasswordCheckState(),
            PasswordSafetyCheckState::kDefault);
  EXPECT_EQ(safety_check_manager_->GetUpdateChromeCheckState(),
            UpdateChromeSafetyCheckState::kOutOfDate);
  EXPECT_EQ(safety_check_manager_->GetSafeBrowsingCheckState(),
            SafeBrowsingSafetyCheckState::kDefault);
}
