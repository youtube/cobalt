// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/metrics/statistics_recorder.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_manager_test_utils_bridge.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/touch_to_fill/password_manager/password_generation/android/touch_to_fill_password_generation_controller.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_results_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

class PasswordManagerAndroidBrowserTest
    : public AndroidBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PasswordManagerAndroidBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // See crbug.com/331746629. The login database on Android will be
    // deprecated soon. So create a fake backend on GMS Core for password
    // storing.
    SetUpGmsCoreFakeBackends();
  }
  ~PasswordManagerAndroidBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpOnMainThread() override {
    // Map all out-going DNS lookups to the local server. This must be used in
    // conjunction with switches::kIgnoreCertificateErrors to work.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Setup HTTPS server serving files from standard test directory.
    static constexpr base::FilePath::CharType kDocRoot[] =
        FILE_PATH_LITERAL("chrome/test/data");
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
    ASSERT_TRUE(https_server_.Start());
  }

  void NavigateToFile(const std::string& file_path) {
    PasswordsNavigationObserver observer(GetActiveWebContents());
    EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       https_server_.GetURL(file_path)));
    ASSERT_TRUE(observer.Wait());
  }

  const GURL& base_url() const { return https_server_.base_url(); }

  void WaitForHistogram(const std::string& histogram_name,
                        const base::HistogramTester& histogram_tester) {
    // Continue if histogram was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram_name)) {
      return;
    }

    // Else, wait until the histogram is recorded.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
    run_loop.Run();
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  void WaitForPasswordStores() {
    scoped_refptr<password_manager::PasswordStoreInterface>
        profile_password_store = ProfilePasswordStoreFactory::GetForProfile(
            GetProfile(), ServiceAccessType::IMPLICIT_ACCESS);
    password_manager::PasswordStoreResultsObserver profile_syncer;
    profile_password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
        profile_syncer.GetWeakPtr());
    profile_syncer.WaitForResults();

    scoped_refptr<password_manager::PasswordStoreInterface>
        account_password_store = AccountPasswordStoreFactory::GetForProfile(
            GetProfile(), ServiceAccessType::IMPLICIT_ACCESS);
    if (account_password_store) {
      password_manager::PasswordStoreResultsObserver account_syncer;
      account_password_store->GetAllLoginsWithAffiliationAndBrandingInformation(
          account_syncer.GetWeakPtr());
      account_syncer.WaitForResults();
    }
  }

 private:
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_P(PasswordManagerAndroidBrowserTest,
                       TriggerFormSubmission) {
  base::HistogramTester uma_recorder;

  password_manager::PasswordStoreInterface* password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          GetProfile(), ServiceAccessType::IMPLICIT_ACCESS)
          .get();

  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = base_url().spec();
  signin_form.url = base_url();
  signin_form.action = base_url();
  signin_form.username_value = u"username";
  signin_form.password_value = u"password";
  password_store->AddLogin(signin_form);
  WaitForPasswordStores();

  bool has_form_tag = GetParam();
  NavigateToFile(has_form_tag ? "/password/simple_password.html"
                              : "/password/no_form_element.html");

  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          GetActiveWebContents()->GetPrimaryMainFrame());

  // There should be only one form with two fields in the test html.
  ASSERT_EQ(static_cast<const password_manager::PasswordManager*>(
                driver->GetPasswordManager())
                ->form_managers()
                .size(),
            1u);

  PasswordsNavigationObserver observer(GetActiveWebContents());
  observer.SetPathToWaitFor("/password/done.html");

  // A user taps the username field.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "document.getElementById('username_field').focus();"));

  // Because on some simulator bots, renderer may take longer time to finish
  // the "focus()" call.
  content::MainThreadFrameObserver frame_observer(
      GetActiveWebContents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  frame_observer.Wait();

  // A user accepts a credential in TouchToFill. That fills in the credential
  // and submits it.
  ChromePasswordManagerClient::FromWebContents(GetActiveWebContents())
      ->StartSubmissionTrackingAfterTouchToFill(u"username");

  driver->FillSuggestion(u"username", u"password", base::DoNothing());
  driver->TriggerFormSubmission();

  ASSERT_TRUE(observer.Wait());

  // Wait for the histogram to be ready to reduce flakiness.
  WaitForHistogram("PasswordManager.TouchToFill.TimeToSuccessfulLogin",
                   uma_recorder);
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 1);
}

// Tests that manual password generation can be triggered on the text field if
// field's name contains "password".
IN_PROC_BROWSER_TEST_P(PasswordManagerAndroidBrowserTest,
                       TriggerPasswordGenerationOnTextField) {
  password_manager::PasswordFormManager::
      set_wait_for_server_predictions_for_filling(false);

  NavigateToFile("/password/sign_in_with_password_type_text.html");

  password_manager::ContentPasswordManagerDriver* driver =
      password_manager::ContentPasswordManagerDriver::GetForRenderFrameHost(
          GetActiveWebContents()->GetPrimaryMainFrame());

  // After parsing, text field is considered as manual generation eligible
  // field.
  EXPECT_TRUE(base::test::RunUntil([driver]() {
    return driver->GetPasswordGenerationHelper()
               ->GenerationEnabledFieldsForTests()
               .size() == 1;
  }));
  const base::flat_set<autofill::FieldRendererId>& generation_enabled_fields =
      driver->GetPasswordGenerationHelper()->GenerationEnabledFieldsForTests();
  autofill::FieldRendererId password_field_renderer_id =
      *generation_enabled_fields.begin();

  // A user taps on the password field. JS call updates the last focused field
  // for `PasswordAutofillAgent`. `PasswordGenerationAgent` uses the last
  // focused field info to fill the manually generated password.
  // TODO: crbug.com/372635030 - Replace JS script and get rid of with
  // `ChromePasswordManagerClient::FocusedInputChanged` once
  // `SimulateMouseClickOrTapElementWithId` call starts creation of keyboard
  // accessory.
  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "document.getElementById('password_field').focus();"));
  // Wait because on some emulator bots the renderer may take longer time to
  // finish the "focus()" call.
  content::MainThreadFrameObserver frame_observer(
      GetActiveWebContents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
  frame_observer.Wait();
  ChromePasswordManagerClient::FromWebContents(GetActiveWebContents())
      ->FocusedInputChanged(
          driver, password_field_renderer_id,
          autofill::mojom::FocusedFieldType::kFillableNonSearchField);
  // User generates the password manually.
  PasswordAccessoryController* password_accessory =
      PasswordAccessoryController::GetIfExisting(GetActiveWebContents());
  ASSERT_TRUE(password_accessory);
  password_accessory->OnOptionSelected(
      autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL);

  PasswordGenerationController* password_generation_controller =
      PasswordGenerationController::GetIfExisting(GetActiveWebContents());

  // Wait till password generation bottomsheet is shown to the user.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return password_generation_controller
        ->GetTouchToFillGenerationControllerForTesting();
  }));
  TouchToFillPasswordGenerationController* touch_to_fill_generation_controller =
      password_generation_controller
          ->GetTouchToFillGenerationControllerForTesting();

  touch_to_fill_generation_controller->OnGeneratedPasswordAccepted(u"Password");

  EXPECT_TRUE(
      content::EvalJs(
          GetActiveWebContents(),
          "document.getElementById('password_field').value === \"Password\"")
          .ExtractBool());
}

INSTANTIATE_TEST_SUITE_P(VariateFormElementPresence,
                         PasswordManagerAndroidBrowserTest,
                         testing::Bool());
