// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_signin_promo_tab_helper.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/views/autofill/address_sign_in_promo_view.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/passwords/password_save_update_view.h"
#include "chrome/browser/ui/views/promos/autofill_bubble_signin_promo_view.h"
#include "chrome/browser/ui/views/promos/bubble_signin_promo_signin_button_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

using autofill::AddressDataManager;
using autofill::AddressSignInPromoView;
using autofill::AutofillProfile;
using autofill::ContentAutofillClient;
using autofill::SaveAddressProfileView;

constexpr char kButton[] = "SignInButton";

using testing::_;
using testing::Return;

std::unique_ptr<KeyedService> BuildMockSyncService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<syncer::MockSyncService>>();
}

}  // namespace

class AutofillBubbleSignInPromoInteractiveUITest : public ManagePasswordsTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ManagePasswordsTest::SetUpInProcessBrowserTestFixture();
    url_loader_factory_helper_.SetUp();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &AutofillBubbleSignInPromoInteractiveUITest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kImprovedSigninUIOnDesktop},
        /*disabled_features=*/{});
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    // Create local password store and mock sync service.
    local_password_store_ = CreateAndUseTestPasswordStore(context);
    SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        context, base::BindRepeating(&BuildMockSyncService));
  }

  void SetUpOnMainThread() override {
    ManagePasswordsTest::SetUpOnMainThread();
    ON_CALL(sync_service_mock(), GetDataTypesForTransportOnlyMode())
        .WillByDefault(Return(syncer::DataTypeSet::All()));
  }

  // Trigger the password save by simulating an "Accept" in the password bubble,
  // and wait for it to appear in the profile store.
  void SavePassword();

  // Address save callback for `TriggerSaveAddressBubble`.
  void SaveAddress(autofill::AutofillClient::AddressPromptUserDecision decision,
                   base::optional_ref<const AutofillProfile> profile);

  // Trigger the address save bubble. This does not save the address yet.
  void TriggerSaveAddressBubble(const AutofillProfile& address);

  // Perform a sign in with the `access_point`.
  void SignIn(signin_metrics::AccessPoint access_point);

  // Returns true if the current tab's URL is a sign in URL.
  bool IsSignInURL();

  // Returns true if there is a primary account without a refresh token in
  // persistent error state.
  bool IsSignedIn();

  // Mock the activation of the sync service upon sign in.
  void ActivateSyncService() {
    ON_CALL(sync_service_mock(), GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    ON_CALL(sync_service_mock(), HasSyncConsent()).WillByDefault(Return(true));
  }

  auto SendKeyPress(ui::KeyboardCode key) {
    return Check([this, key]() {
      return ui_test_utils::SendKeyPressSync(browser(), key, false, false,
                                             false, false);
    });
  }

  // Add additional account info for pixel tests.
  void ExtendAccountInfo(AccountInfo& info);

  ContentAutofillClient& client() const {
    return *ContentAutofillClient::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  AddressDataManager& address_data_manager() const {
    return client().GetPersonalDataManager().address_data_manager();
  }

  syncer::MockSyncService& sync_service_mock() {
    return *static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->profile()));
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return url_loader_factory_helper_.test_url_loader_factory();
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  ChromeSigninClientWithURLLoaderHelper url_loader_factory_helper_;
  base::CallbackListSubscription create_services_subscription_;
  scoped_refptr<password_manager::TestPasswordStore> local_password_store_;
};

void AutofillBubbleSignInPromoInteractiveUITest::SavePassword() {
  password_manager::PasswordStoreWaiter store_waiter(
      local_password_store_.get());

  PasswordBubbleViewBase* bubble =
      PasswordBubbleViewBase::manage_password_bubble();
  bubble->AcceptDialog();

  store_waiter.WaitOrReturn();
}

void AutofillBubbleSignInPromoInteractiveUITest::SaveAddress(
    autofill::AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> profile) {
  address_data_manager().AddProfile(*profile);
}

void AutofillBubbleSignInPromoInteractiveUITest::TriggerSaveAddressBubble(
    const AutofillProfile& address) {
  client().ConfirmSaveAddressProfile(
      address, nullptr, false,
      base::BindOnce(&AutofillBubbleSignInPromoInteractiveUITest::SaveAddress,
                     base::Unretained(this)));
}

void AutofillBubbleSignInPromoInteractiveUITest::SignIn(
    signin_metrics::AccessPoint access_point) {
  ActivateSyncService();
  signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .WithAccessPoint(access_point)
          .AsPrimary(signin::ConsentLevel::kSignin)
          .Build("test@email.com"));
}

bool AutofillBubbleSignInPromoInteractiveUITest::IsSignInURL() {
  DiceTabHelper* tab_helper = DiceTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  return tab_helper->IsChromeSigninPage();
}

bool AutofillBubbleSignInPromoInteractiveUITest::IsSignedIn() {
  return signin_util::GetSignedInState(identity_manager()) ==
         signin_util::SignedInState::kSignedIn;
}

void AutofillBubbleSignInPromoInteractiveUITest::ExtendAccountInfo(
    AccountInfo& info) {
  info.given_name = "FirstName";
  info.full_name = "FirstName LastName";
  signin::UpdateAccountInfoForAccount(identity_manager(), info);
}

/////////////////////////////////////////////////////////////////
///// Password Sign in Promo

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       PasswordSignInPromoNoAccountPresent) {
  base::HistogramTester histogram_tester;

  // Set up password and the local password store.
  GetController()->OnPasswordSubmitted(
      CreateFormManager(local_password_store_.get(), nullptr));

  // Save the password and check that it was properly saved to profile store.
  SavePassword();
  EXPECT_EQ(1u, local_password_store_->stored_passwords().size());

  // Wait for the bubble to be replaced with the sign in promo and click the
  // sign in button.
  RunTestSequence(
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsurePresent(PasswordSaveUpdateView::kPasswordBubble),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(PasswordSaveUpdateView::kPasswordBubble, std::string(),
                 "5455375"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(PasswordSaveUpdateView::kPasswordBubble));

  // Check that clicking the sign in button navigated to a sign in page.
  EXPECT_TRUE(IsSignInURL());

  // Check that there is a helper attached to the sign in tab, because the
  // password still needs to be moved.
  EXPECT_TRUE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());

  // This would move the password to account storage.
  EXPECT_CALL(sync_service_mock(), SelectTypeAndMigrateLocalDataItemsWhenActive(
                                       syncer::PASSWORDS, _))
      .Times(1);

  // Simulate a sign in event with the correct access point, which should call
  // `SelectTypeAndMigrateLocalDataItemsWhenActive()`.
  SignIn(signin_metrics::AccessPoint::kPasswordBubble);

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Signin metrics - Offered/Started/Completed are recorded, but no values for
  // WebSignin (WithDefault).
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kPasswordBubble, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount",
      signin_metrics::AccessPoint::kPasswordBubble, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kPasswordBubble, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kPasswordBubble,
      1);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);

  histogram_tester.ExpectBucketCount(
      "Signin.SignInPromo.Accepted",
      signin_metrics::AccessPoint::kPasswordBubble, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       PasswordSignInPromoWithWebSignedInAccount) {
  base::HistogramTester histogram_tester;

  // Sign in with an account, but only on the web. The primary account is not
  // set.
  AccountInfo info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build("test@email.com"));
  ExtendAccountInfo(info);

  // Set up password and the local password store.
  GetController()->OnPasswordSubmitted(
      CreateFormManager(local_password_store_.get(), nullptr));

  // Save the password and check that it was properly saved to profile store.
  SavePassword();
  EXPECT_EQ(1u, local_password_store_->stored_passwords().size());

  // This would move the password to account storage.
  EXPECT_CALL(sync_service_mock(), SelectTypeAndMigrateLocalDataItemsWhenActive(
                                       syncer::PASSWORDS, _))
      .Times(1);

  // Wait for the bubble to be replaced with the sign in promo and click the
  // sign in button. This should directly sign the user in and trigger the data
  // migration.
  ActivateSyncService();
  RunTestSequence(
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsurePresent(PasswordSaveUpdateView::kPasswordBubble),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(PasswordSaveUpdateView::kPasswordBubble, std::string(),
                 "5455375"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(PasswordSaveUpdateView::kPasswordBubble));

  // Check that there is no helper attached to the sign in tab, because the
  // password was already moved.
  EXPECT_FALSE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                   *browser()->tab_strip_model()->GetActiveWebContents())
                   ->IsInitializedForTesting());

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Signin metrics - WebSignin (WithDefault) metrics are also recorded.
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kPasswordBubble, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kPasswordBubble,
      1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kPasswordBubble, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered.WithDefault",
      signin_metrics::AccessPoint::kPasswordBubble, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.WebSignin.SourceToChromeSignin",
      signin_metrics::AccessPoint::kPasswordBubble, 1);

  histogram_tester.ExpectBucketCount(
      "Signin.SignInPromo.Accepted",
      signin_metrics::AccessPoint::kPasswordBubble, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       PasswordSignInPromoWithAccountSignInPending) {
  // Sign in with an account, and put its refresh token into an error
  // state. This simulates the "sign in pending" state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  ExtendAccountInfo(info);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Set up password and the local password store.
  GetController()->OnPasswordSubmitted(
      CreateFormManager(local_password_store_.get(), nullptr));

  // Start recording metrics after signing in.
  base::HistogramTester histogram_tester;

  // Save the password and check that it was properly saved to profile store.
  SavePassword();
  EXPECT_EQ(1u, local_password_store_->stored_passwords().size());

  // Wait for the bubble to be replaced with the sign in promo and click
  // the sign in button.
  RunTestSequence(
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsurePresent(PasswordSaveUpdateView::kPasswordBubble),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(PasswordSaveUpdateView::kPasswordBubble, std::string(),
                 "5455375"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(PasswordSaveUpdateView::kPasswordBubble));

  // Check that clicking the sign in button navigated to a sign in page.
  EXPECT_TRUE(IsSignInURL());

  // Check that there is a helper attached to the sign in tab, because the
  // password still needs to be moved.
  EXPECT_TRUE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());
  EXPECT_FALSE(IsSignedIn());

  // This would move the password to account storage.
  EXPECT_CALL(sync_service_mock(), SelectTypeAndMigrateLocalDataItemsWhenActive(
                                       syncer::PASSWORDS, _))
      .Times(1);

  // Set a new refresh token for the primary account, which verifies the
  // user's identity and signs them back in. This triggers the local data
  // migration.
  ActivateSyncService();
  identity_manager()->GetAccountsMutator()->AddOrUpdateAccount(
      info.gaia, info.email, "dummy_refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::AccessPoint::kPasswordBubble,
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Signin metrics - nothing should be recorded for reauth.
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Completed", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);

  histogram_tester.ExpectBucketCount(
      "Signin.SignInPromo.Accepted",
      signin_metrics::AccessPoint::kPasswordBubble, 1);
}

/////////////////////////////////////////////////////////////////
///// Address Sign in Promo

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       AddressSignInPromoNoAccountPresent) {
  base::HistogramTester histogram_tester;

  // Trigger the address save bubble.
  AutofillProfile address = autofill::test::GetFullProfile();
  TriggerSaveAddressBubble(address);

  // Accept the save bubble, wait for it to be replaced with the sign in promo
  // and click the sign in button.
  RunTestSequence(
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),
      EnsurePresent(AddressSignInPromoView::kBubbleFrameViewId),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(AddressSignInPromoView::kBubbleFrameViewId, std::string(),
                 "5860426"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(AddressSignInPromoView::kBubbleFrameViewId));

  // Check that clicking the sign in button navigated to a sign in page.
  EXPECT_TRUE(IsSignInURL());

  // Check that there is a helper attached to the sign in tab, because the
  // address still needs to be moved.
  EXPECT_TRUE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());

  // This would move the address to account storage.
  std::vector<syncer::LocalDataItemModel::DataId> items{address.guid()};
  EXPECT_CALL(sync_service_mock(), SelectTypeAndMigrateLocalDataItemsWhenActive(
                                       syncer::CONTACT_INFO, items))
      .Times(1);

  // Simulate a sign in event with the correct access point, which will move the
  // address.
  SignIn(signin_metrics::AccessPoint::kAddressBubble);

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Signin metrics - Offered/Started/Completed are recorded, but no values for
  // WebSignin (WithDefault).
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kAddressBubble, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount",
      signin_metrics::AccessPoint::kAddressBubble, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Started", signin_metrics::AccessPoint::kAddressBubble, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kAddressBubble,
      1);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);

  histogram_tester.ExpectBucketCount(
      "Signin.SignInPromo.Accepted",
      signin_metrics::AccessPoint::kAddressBubble, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       AddressSignInPromoWithWebSignedInAccount) {
  base::HistogramTester histogram_tester;

  // Sign in with an account, but only on the web. The primary account is not
  // set.
  AccountInfo info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build("test@email.com"));
  ExtendAccountInfo(info);

  // Trigger the address save bubble.
  AutofillProfile address = autofill::test::GetFullProfile();
  TriggerSaveAddressBubble(address);

  // This would move the address to account storage.
  std::vector<syncer::LocalDataItemModel::DataId> items{address.guid()};
  EXPECT_CALL(sync_service_mock(), SelectTypeAndMigrateLocalDataItemsWhenActive(
                                       syncer::CONTACT_INFO, items))
      .Times(1);

  // Accept the save bubble, wait for the save bubble to be replaced with the
  // sign in promo and click the sign in button. This should directly sign the
  // user in and move the address.
  ActivateSyncService();
  RunTestSequence(
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),
      EnsurePresent(AddressSignInPromoView::kBubbleFrameViewId),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(AddressSignInPromoView::kBubbleFrameViewId, std::string(),
                 "5860426"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(AddressSignInPromoView::kBubbleFrameViewId));

  // Check that there is no helper attached to the sign in tab, because the
  // password was already moved.
  EXPECT_FALSE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                   *browser()->tab_strip_model()->GetActiveWebContents())
                   ->IsInitializedForTesting());

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Signin metrics - WebSignin (WithDefault) metrics are also recorded.
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kAddressBubble, 1);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Completed", signin_metrics::AccessPoint::kAddressBubble,
      1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered", signin_metrics::AccessPoint::kAddressBubble, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.SignIn.Offered.WithDefault",
      signin_metrics::AccessPoint::kAddressBubble, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectBucketCount(
      "Signin.WebSignin.SourceToChromeSignin",
      signin_metrics::AccessPoint::kAddressBubble, 1);

  histogram_tester.ExpectBucketCount(
      "Signin.SignInPromo.Accepted",
      signin_metrics::AccessPoint::kAddressBubble, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       AddressSignInPromoWithAccountSignInPending) {
  // Sign in with an account, and put its refresh token into an error
  // state. This simulates the "sign in pending" state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  ExtendAccountInfo(info);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Start recording metrics after signing in.
  base::HistogramTester histogram_tester;

  // Trigger the address save bubble.
  AutofillProfile address = autofill::test::GetFullProfile();
  TriggerSaveAddressBubble(address);

  // Accept the save bubble, wait for the save bubble to be replaced with the
  // sign in promo and click the sign in button.
  RunTestSequence(
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),
      EnsurePresent(AddressSignInPromoView::kBubbleFrameViewId),
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshot can only run in pixel_tests on Windows."),
      Screenshot(AddressSignInPromoView::kBubbleFrameViewId, std::string(),
                 "5860426"),
      NameChildViewByType<views::MdTextButton>(
          BubbleSignInPromoSignInButtonView::kPromoSignInButton, kButton),
      PressButton(kButton).SetMustRemainVisible(false),
      EnsureNotPresent(AddressSignInPromoView::kBubbleFrameViewId));

  // Check that clicking the sign in button navigated to a sign in page.
  EXPECT_TRUE(IsSignInURL());

  // Check that there is a helper attached to the sign in tab, because the
  // address still needs to be moved.
  EXPECT_TRUE(autofill::AutofillSigninPromoTabHelper::GetForWebContents(
                  *browser()->tab_strip_model()->GetActiveWebContents())
                  ->IsInitializedForTesting());

  // This would move the address to account storage.
  std::vector<syncer::LocalDataItemModel::DataId> items{address.guid()};
  EXPECT_CALL(sync_service_mock(), SelectTypeAndMigrateLocalDataItemsWhenActive(
                                       syncer::CONTACT_INFO, items))
      .Times(1);

  // Set a new refresh token for the primary account, which verifies the
  // user's identity and signs them back in. This would trigger the data
  // migration.
  ActivateSyncService();
  identity_manager()->GetAccountsMutator()->AddOrUpdateAccount(
      info.gaia, info.email, "dummy_refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::AccessPoint::kAddressBubble,
      signin_metrics::SourceForRefreshTokenOperation::
          kDiceResponseHandler_Signin);

  // Check that the sign in was successful.
  EXPECT_TRUE(IsSignedIn());

  // Signin metrics - nothing should be recorded for reauth.
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Completed", 0);
  histogram_tester.ExpectTotalCount("Signin.SignIn.Offered.WithDefault", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.SignIn.Offered.NewAccountNoExistingAccount", 0);
  histogram_tester.ExpectTotalCount("Signin.WebSignin.SourceToChromeSignin", 0);

  histogram_tester.ExpectBucketCount(
      "Signin.SignInPromo.Accepted",
      signin_metrics::AccessPoint::kAddressBubble, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       AddressSignInPromoDismissedEscapeKey) {
  base::HistogramTester histogram_tester;

  // Sign in with an account, and put its refresh token into an error
  // state. This simulates the "sign in pending" state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  ExtendAccountInfo(info);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Trigger the address save bubble.
  AutofillProfile address = autofill::test::GetFullProfile();
  TriggerSaveAddressBubble(address);

  // Accept the save bubble, wait for the save bubble to be replaced with the
  // sign in promo and dismiss it.
  RunTestSequence(
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),
      EnsurePresent(AddressSignInPromoView::kBubbleFrameViewId),
      // Click the promo to put the bubble into focus.
      MoveMouseTo(AddressSignInPromoView::kBubbleFrameViewId), ClickMouse(),
      SendKeyPress(ui::VKEY_ESCAPE),
      WaitForHide(AddressSignInPromoView::kBubbleFrameViewId));

  histogram_tester.ExpectBucketCount(
      "Signin.SignInPromo.DismissedEscapeKey",
      signin_metrics::AccessPoint::kAddressBubble, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillBubbleSignInPromoInteractiveUITest,
                       AddressSignInPromoDismissedCloseButton) {
  base::HistogramTester histogram_tester;

  // Sign in with an account, and put its refresh token into an error
  // state. This simulates the "sign in pending" state.
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", signin::ConsentLevel::kSignin);
  ExtendAccountInfo(info);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Trigger the address save bubble.
  AutofillProfile address = autofill::test::GetFullProfile();
  TriggerSaveAddressBubble(address);

  // Accept the save bubble, wait for the save bubble to be replaced with the
  // sign in promo and dismiss it.
  RunTestSequence(
      PressButton(views::DialogClientView::kOkButtonElementId),
      WaitForEvent(BubbleSignInPromoSignInButtonView::kPromoSignInButton,
                   kBubbleSignInPromoSignInButtonHasCallback),
      EnsureNotPresent(SaveAddressProfileView::kTopViewId),
      EnsurePresent(AddressSignInPromoView::kBubbleFrameViewId),
      PressButton(views::BubbleFrameView::kCloseButtonElementId),
      WaitForHide(AddressSignInPromoView::kBubbleFrameViewId));

  histogram_tester.ExpectBucketCount(
      "Signin.SignInPromo.DismissedCloseButton",
      signin_metrics::AccessPoint::kAddressBubble, 1);
}
