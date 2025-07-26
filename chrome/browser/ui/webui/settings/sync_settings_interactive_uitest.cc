// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

class SyncSettingsInteractiveTest
    : public SigninBrowserTestBaseT<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  SyncSettingsInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kImprovedSettingsUIOnDesktop},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SyncSettingsInteractiveTest,
                       PressingSignOutButtonsSignsOutUser) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);

  const DeepQuery turn_off_button_query = {"settings-ui",
                                           "settings-main",
                                           "settings-basic-page",
                                           "settings-people-page",
                                           "settings-sync-account-control",
                                           "cr-button#signout-button"};

  const DeepQuery drop_down_query = {"settings-ui",
                                     "settings-main",
                                     "settings-basic-page",
                                     "settings-people-page",
                                     "settings-sync-account-control",
                                     "cr-icon-button#dropdown-arrow"};

  std::unique_ptr<content::TestNavigationObserver> observer;
  if (switches::IsImprovedSigninUIOnDesktopEnabled()) {
    auto url = GURL(chrome::kChromeUISignoutConfirmationURL);
    observer = std::make_unique<content::TestNavigationObserver>(url);
    observer->StartWatchingNewWebContents();
  }

  RunTestSequence(
      Do([&]() {
        identity_test_env()->MakePrimaryAccountAvailable(
            "kTestEmail@email.com", signin::ConsentLevel::kSignin);
      }),
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome::GetSettingsUrl(
                                                 chrome::kSyncSetupSubPage))),
      ExecuteJsAt(kFirstTabContents, drop_down_query,
                  "e => e.visibility === \"hidden\""),
      ExecuteJsAt(kFirstTabContents, turn_off_button_query, "e => e.click()"));

  if (observer.get()) {
    observer->Wait();
    auto* signin_view_controller = browser()->signin_view_controller();
    CHECK(signin_view_controller->ShowsModalDialog());

    auto* signout_ui = SignoutConfirmationUI::GetForTesting(
        signin_view_controller->GetModalDialogWebContentsForTesting());
    ASSERT_TRUE(signout_ui);
    signout_ui->AcceptDialogForTesting();
  }

  ASSERT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
}
