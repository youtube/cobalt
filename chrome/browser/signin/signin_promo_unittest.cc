// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

#if !BUILDFLAG(IS_CHROMEOS)
TEST(SigninPromoTest, TestPromoURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr("access_point=0&reason=0&auto_close=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(signin_metrics::AccessPoint::kStartPage,
                          signin_metrics::Reason::kSigninPrimaryAccount, true));
  replace_query.SetQueryStr("access_point=15&reason=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(signin_metrics::AccessPoint::kSigninPromo,
                          signin_metrics::Reason::kAddSecondaryAccount, false));
}

TEST(SigninPromoTest, TestReauthURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr(
      "access_point=0&reason=6&auto_close=1"
      "&email=example%40domain.com&validateEmail=1"
      "&readOnlyEmail=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedReauthURLWithEmail(signin_metrics::AccessPoint::kStartPage,
                                    signin_metrics::Reason::kFetchLstOnly,
                                    "example@domain.com"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST(SigninPromoTest, SigninURLForDice) {
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "color_scheme=dark&flow=promo",
      GetChromeSyncURLForDice(
          {.request_dark_scheme = true, .flow = Flow::PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "email_hint=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetChromeSyncURLForDice(
          {"email@gmail.com", GURL("https://continue_url/")}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/"
      "sync?ssp=1&flow=embedded_promo",
      GetChromeSyncURLForDice({.flow = Flow::EMBEDDED_PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/AddSession?"
      "Email=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetAddAccountURLForDice("email@gmail.com",
                              GURL("https://continue_url/")));
}

class ShowPromoTest : public testing::Test {
 public:
  ShowPromoTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<syncer::MockSyncService>());
        }));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  }

  syncer::MockSyncService* sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  IdentityManager* identity_manager() {
    return identity_test_env_adaptor_->identity_test_env()->identity_manager();
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};


TEST_F(ShowPromoTest, DoNotShowAddressSignInPromoWithoutImprovedBrowserSignin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{switches::kImprovedSigninUIOnDesktop});

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(),
                                            autofill::test::StandardProfile()));
}

#if !BUILDFLAG(IS_ANDROID)
class ShowSyncPromoTest : public ShowPromoTest {
 protected:
  void DisableSync() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(syncer::kDisableSync);
  }
};

// Verifies that ShouldShowSyncPromo returns false if sync is disabled by
// policy.
TEST_F(ShowSyncPromoTest, ShouldShowSyncPromoSyncDisabled) {
  DisableSync();
  EXPECT_FALSE(ShouldShowSyncPromo(*profile()));
}

// Verifies that ShouldShowSyncPromo returns true if all conditions to
// show the promo are met.
TEST_F(ShowSyncPromoTest, ShouldShowSyncPromoSyncEnabled) {
#if BUILDFLAG(IS_CHROMEOS)
  // No sync promo on Ash.
  EXPECT_FALSE(ShouldShowSyncPromo(*profile()));
#else
  EXPECT_TRUE(ShouldShowSyncPromo(*profile()));
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ShowSyncPromoTest, ShowPromoWithSignedInAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  EXPECT_TRUE(ShouldShowSyncPromo(*profile()));
}

TEST_F(ShowSyncPromoTest, DoNotShowPromoWithSyncingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSync);
  EXPECT_FALSE(ShouldShowSyncPromo(*profile()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ShowSigninPromoTestExplicitBrowserSignin : public ShowPromoTest {
 public:
  void SetUp() override {
    ShowPromoTest::SetUp();
    feature_list.InitWithFeatures(
        /*enabled_features=*/{switches::kImprovedSigninUIOnDesktop},
        /*disabled_features=*/{});
    ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
        .WillByDefault(testing::Return(syncer::DataTypeSet::All()));
  }

  GaiaId gaia_id() {
    return identity_manager()
        ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
        .gaia;
  }

  autofill::AutofillProfile CreateAddress(
      const std::string& country_code = "US") {
    return autofill::test::StandardProfile(AddressCountryCode(country_code));
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

TEST_F(ShowSigninPromoTestExplicitBrowserSignin, ShowPromoWithNoAccount) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoWithWebSignedInAccount) {
  MakeAccountAvailable(identity_manager(), "test@email.com");
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoWithSignInPausedAccount) {
  AccountInfo info = MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", ConsentLevel::kSignin);
  UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithAlreadySignedInAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithAlreadySyncingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSync);
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithOffTheRecordProfile) {
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(
      *profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithLocalSyncEnabled) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetBoolean(syncer::prefs::kEnableLocalSyncBackend,
                                    true);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithoutSyncAllowed) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  ON_CALL(*sync_service(), GetDisableReasons())
      .WillByDefault(testing::Return(syncer::SyncService::DisableReasonSet(
          {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY})));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithTypeManagedByPolicy) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  ON_CALL(*sync_service()->GetMockUserSettings(),
          IsTypeManagedByPolicy(syncer::UserSelectableType::kPasswords))
      .WillByDefault(testing::Return(true));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithoutTransportOnlyDataType) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet()));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPasswordPromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowAddressPromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoAfterTwoTimesDismissed) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 2);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoAfterTwoTimesDismissedByDifferentAccounts) {
  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 1);
  SigninPrefs prefs(*profile()->GetPrefs());
  prefs.IncrementAutofillSigninPromoDismissCount(GaiaId("gaia_id"));

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowAddressIfProfileMigrationBlocked) {
  autofill::AutofillProfile address = autofill::test::StandardProfile();
  autofill::PersonalDataManagerFactory::GetForBrowserContext(profile())
      ->address_data_manager()
      .AddMaxStrikesToBlockProfileMigration(address.guid());
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), address));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowAddressIfCountryNotEligibleForAccountStorage) {
  const std::string non_eligible_country_code("IR");

  ASSERT_FALSE(
      autofill::PersonalDataManagerFactory::GetForBrowserContext(profile())
          ->address_data_manager()
          .IsCountryEligibleForAccountStorage(non_eligible_country_code));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(
      *profile(), CreateAddress(non_eligible_country_code)));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       RecordSignInPromoShownWithoutAccount) {
  // Add an account without cookies. The per-profile pref will be recorded.
  AccountInfo account =
      MakeAccountAvailable(identity_manager(), "test@email.com");

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile());

  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kPasswordSignInPromoShownCountPerProfile));
  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kAddressSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       RecordSignInPromoShownWithAccount) {
  // Test setup for adding an account with cookies.
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  // Add an account with cookies, which will record the per-account prefs.
  AccountInfo account = identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithAccessPoint(signin_metrics::AccessPoint::kUnknown)
          .WithCookie(true)
          .Build("test@email.com"));

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile.get());

  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kPasswordSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kAddressSignInPromoShownCountPerProfile));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
