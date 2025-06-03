// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_test.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/test_utils.h"

using base::ASCIIToUTF16;
using password_manager::PasswordFormManager;
using testing::Return;
using testing::ReturnRef;

namespace {
constexpr char16_t kTestUsername[] = u"test_username";
}  // namespace

ManagePasswordsTest::ManagePasswordsTest() {
  fetcher_.Fetch();

  // Turn off waiting for server predictions in order to avoid dealing with
  // posted tasks in PasswordFormManager.
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
}

ManagePasswordsTest::~ManagePasswordsTest() = default;

void ManagePasswordsTest::SetUpOnMainThread() {
  InteractiveBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");

  password_form_.signon_realm = test_url.GetWithEmptyPath().spec();
  password_form_.url = test_url;
  password_form_.username_value = kTestUsername;
  password_form_.password_value = u"test_password";
  password_form_.match_type = password_manager::PasswordForm::MatchType::kExact;

  ASSERT_TRUE(AddTabAtIndex(0, test_url, ui::PAGE_TRANSITION_TYPED));
}

void ManagePasswordsTest::SetUpInProcessBrowserTestFixture() {
  InteractiveBrowserTest::SetUpInProcessBrowserTestFixture();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating([](content::BrowserContext* context) {
                // Overwrite the password store early before it's accessed by
                // safe browsing.
                ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
                    context,
                    base::BindRepeating(&password_manager::BuildPasswordStore<
                                        content::BrowserContext,
                                        password_manager::TestPasswordStore>));

                SyncServiceFactory::GetInstance()->SetTestingFactory(
                    context,
                    base::BindRepeating([](content::BrowserContext*)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<syncer::TestSyncService>();
                    }));
              }));
}

void ManagePasswordsTest::ExecuteManagePasswordsCommand() {
  // Show the window to ensure that it's active.
  browser()->window()->Show();

  CommandUpdater* updater = browser()->command_controller();
  EXPECT_TRUE(updater->IsCommandEnabled(IDC_MANAGE_PASSWORDS_FOR_PAGE));
  EXPECT_TRUE(updater->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE));
}

void ManagePasswordsTest::SetupManagingPasswords() {
  password_manager::PasswordForm federated_form;
  federated_form.signon_realm = "federation://" +
                                embedded_test_server()->GetOrigin().host() +
                                "/somelongeroriginurl.com";
  federated_form.url = embedded_test_server()->GetURL("/empty.html");
  federated_form.federation_origin =
      url::Origin::Create(GURL("https://somelongeroriginurl.com/"));
  federated_form.username_value = u"test_federation_username";
  federated_form.match_type = password_manager::PasswordForm::MatchType::kExact;
  std::vector<const password_manager::PasswordForm*> forms = {&password_form_,
                                                              &federated_form};
  GetController()->OnPasswordAutofilled(
      forms, embedded_test_server()->GetOrigin(), nullptr);
}

void ManagePasswordsTest::SetupPendingPassword() {
  GetController()->OnPasswordSubmitted(CreateFormManager());
}

void ManagePasswordsTest::SetupAutomaticPassword() {
  GetController()->OnAutomaticPasswordSave(CreateFormManager(),
                                           /*is_update_confirmation=*/false);
}

void ManagePasswordsTest::SetupAutoSignin(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        local_credentials) {
  ASSERT_FALSE(local_credentials.empty());
  url::Origin origin = url::Origin::Create(local_credentials[0]->url);
  GetController()->OnAutoSignin(std::move(local_credentials), origin);
}

void ManagePasswordsTest::SetupSafeState() {
  browser()->profile()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  SetupPendingPassword();
  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  password_store->AddLogin(password_form_);
  GetController()->SavePassword(password_form_.username_value,
                                password_form_.password_value);
  GetController()->OnBubbleHidden();
  PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());

  EXPECT_EQ(GetController()->GetState(),
            password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);
}

void ManagePasswordsTest::SetupMoreToFixState() {
  browser()->profile()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  // This is an unrelated insecure credential that should still be fixed.
  password_manager::PasswordForm to_be_fixed = password_form_;
  to_be_fixed.signon_realm = "https://somesite.com/";
  to_be_fixed.password_issues.insert({password_manager::InsecureType::kLeaked,
                                      password_manager::InsecurityMetadata()});
  password_store->AddLogin(to_be_fixed);
  password_store->AddLogin(password_form_);
  SetupPendingPassword();
  GetController()->SavePassword(password_form_.username_value,
                                password_form_.password_value);
  GetController()->OnBubbleHidden();
  PasswordManagerBrowserTestBase::WaitForPasswordStore(browser());

  EXPECT_EQ(GetController()->GetState(),
            password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);
}

void ManagePasswordsTest::SetupMovingPasswords() {
  auto form_manager = std::make_unique<
      testing::NiceMock<password_manager::MockPasswordFormManagerForUI>>();
  password_manager::MockPasswordFormManagerForUI* form_manager_ptr =
      form_manager.get();
  std::vector<const password_manager::PasswordForm*> best_matches = {
      test_form()};
  EXPECT_CALL(*form_manager, GetBestMatches).WillOnce(ReturnRef(best_matches));
  ON_CALL(*form_manager, GetPendingCredentials)
      .WillByDefault(ReturnRef(*test_form()));
  ON_CALL(*form_manager, GetFederatedMatches)
      .WillByDefault(
          Return(std::vector<const password_manager::PasswordForm*>{}));
  ON_CALL(*form_manager, GetURL).WillByDefault(ReturnRef(test_form()->url));
  GetController()->OnShowMoveToAccountBubble(std::move(form_manager));
  // Clearing the mock here ensures that |GetBestMatches| won't be called with a
  // reference to |best_matches|.
  testing::Mock::VerifyAndClear(form_manager_ptr);
}

void ManagePasswordsTest::ConfigurePasswordSync(bool is_enabled) {
  // Some tests (such as move password to account) require a signed in users.
  // Make sure there is always one.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@email.com",
      is_enabled ? signin::ConsentLevel::kSync : signin::ConsentLevel::kSignin);

  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(browser()->profile()));
  sync_service->SetAccountInfo(info);
  sync_service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);

  if (is_enabled) {
    sync_service->SetHasSyncConsent(true);
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableType::kPasswords});
  } else {
    sync_service->SetHasSyncConsent(false);
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/syncer::UserSelectableTypeSet());
  }
}

std::unique_ptr<base::HistogramSamples> ManagePasswordsTest::GetSamples(
    const char* histogram) {
  // Ensure that everything has been properly recorded before pulling samples.
  content::RunAllPendingInMessageLoop();
  return histogram_tester_.GetHistogramSamplesSinceCreation(histogram);
}

ManagePasswordsUIController* ManagePasswordsTest::GetController() {
  return ManagePasswordsUIController::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
}

std::unique_ptr<PasswordFormManager> ManagePasswordsTest::CreateFormManager() {
  autofill::FormData observed_form;
  observed_form.url = password_form_.url;
  autofill::FormFieldData field;
  field.form_control_type = autofill::FormControlType::kInputText;
  observed_form.fields.push_back(field);
  field.form_control_type = autofill::FormControlType::kInputPassword;
  observed_form.fields.push_back(field);

  auto form_manager = std::make_unique<PasswordFormManager>(
      &client_, driver_.AsWeakPtr(), observed_form, &fetcher_,
      std::make_unique<password_manager::PasswordSaveManagerImpl>(
          /*profile_form_saver=*/std::make_unique<
              password_manager::StubFormSaver>(),
          /*account_form_saver=*/nullptr),
      /*metrics_recorder=*/nullptr);

  insecure_credential_ = password_form_;
  insecure_credential_.password_issues.insert(
      {password_manager::InsecureType::kLeaked,
       password_manager::InsecurityMetadata(
           base::Time(), password_manager::IsMuted(false),
           password_manager::TriggerBackendNotification(false))});
  fetcher_.set_insecure_credentials({&insecure_credential_});

  fetcher_.NotifyFetchCompleted();

  autofill::FormData submitted_form = observed_form;
  submitted_form.fields[1].value = u"new_password";
  form_manager->ProvisionallySave(submitted_form, &driver_,
                                  nullptr /* possible_username */);

  return form_manager;
}
