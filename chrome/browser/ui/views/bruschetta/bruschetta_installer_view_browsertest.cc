// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bruschetta/bruschetta_installer_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::AnyNumber;
using testing::AtLeast;

namespace bruschetta {
namespace {

class BruschettaInstallerMock : public bruschetta::BruschettaInstaller {
 public:
  MOCK_METHOD(void, Cancel, ());
  MOCK_METHOD(void, Install, (std::string, std::string));
  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));

  MOCK_METHOD(const base::Uuid&, GetDownloadGuid, (), (const));

  MOCK_METHOD(void,
              DownloadStarted,
              (const std::string& guid,
               download::DownloadParams::StartResult result));
  MOCK_METHOD(void, DownloadFailed, ());
  MOCK_METHOD(void,
              DownloadSucceeded,
              (const download::CompletionInfo& completion_info));
};

class BruschettaInstallerViewBrowserTest : public DialogBrowserTest {
 public:
  BruschettaInstallerViewBrowserTest() = default;

  // Disallow copy and assign.
  BruschettaInstallerViewBrowserTest(
      const BruschettaInstallerViewBrowserTest&) = delete;
  BruschettaInstallerViewBrowserTest& operator=(
      const BruschettaInstallerViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    base::Value::Dict pref;

    base::Value::Dict config;
    config.Set(prefs::kPolicyEnabledKey,
               static_cast<int>(prefs::PolicyEnabledState::INSTALL_ALLOWED));
    config.Set(prefs::kPolicyNameKey, "Config name");

    pref.Set("test-config", std::move(config));
    browser()->profile()->GetPrefs()->SetDict(prefs::kBruschettaVMConfiguration,
                                              std::move(pref));
  }

  void ShowUi(const std::string& name) override {
    BruschettaInstallerView::Show(browser()->profile(), GetBruschettaAlphaId());
    view_ = BruschettaInstallerView::GetActiveViewForTesting();

    ASSERT_NE(nullptr, view_);
    ASSERT_FALSE(view_->GetWidget()->IsClosed());

    // Since expectations need to be set ahead of time, but the installer itself
    // expects to create and recreate installers as needed, we create our mocks
    // ahead of time, then each time we're called we transfer our pending mock
    // to the installer and pre-create another for the next call.
    // Note: This means all expectations need to be set before calling install,
    // you can't start installing, set expectation, then cancel/err/etc.
    installer_ = std::make_unique<BruschettaInstallerMock>();
    view_->set_installer_factory_for_testing(base::BindLambdaForTesting(
        [this](Profile* p, base::OnceClosure closure) {
          auto mock_installer = std::move(installer_);
          installer_ = std::make_unique<BruschettaInstallerMock>();
          EXPECT_CALL(*installer_, AddObserver).Times(AnyNumber());
          return static_cast<std::unique_ptr<BruschettaInstaller>>(
              std::move(mock_installer));
        }));
  }

  raw_ptr<BruschettaInstallerView, ExperimentalAsh> view_;
  std::unique_ptr<bruschetta::BruschettaInstallerMock> installer_;
};

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, Show) {
  ShowUi("default");
  EXPECT_NE(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(
      view_->GetPrimaryMessage(),
      l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_CONFIRMATION_TITLE));
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest,
                       CancelOnPromptScreen) {
  ShowUi("default");
  view_->CancelDialog();
  ASSERT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, InstallThenCancel) {
  ShowUi("default");
  EXPECT_CALL(*installer_, Install);
  EXPECT_CALL(*installer_, Cancel).Times(AtLeast(1));

  view_->AcceptDialog();
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));

  view_->CancelDialog();
  ASSERT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, InstallThenError) {
  ShowUi("default");
  base::RunLoop run_loop;
  EXPECT_CALL(*installer_, Install);
  EXPECT_CALL(*installer_, Cancel).Times(AtLeast(1));
  EXPECT_FALSE(view_->progress_bar_for_testing()->GetVisible());

  // Accept, then we're in the installing state.
  view_->AcceptDialog();
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_TRUE(view_->progress_bar_for_testing()->GetVisible());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));

  // Fail, then we're in the cleaning up state.
  view_->Error(BruschettaInstallResult::kStartVmFailed);
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_EQ(nullptr, view_->GetCancelButton());
  EXPECT_TRUE(view_->progress_bar_for_testing()->GetVisible());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ERROR_TITLE));
  EXPECT_EQ(
      view_->GetSecondaryMessage(),
      l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_CLEANING_UP_MESSAGE));

  // Run cleanup to completion, now we're in the error state.
  run_loop.RunUntilIdle();
  EXPECT_NE(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_FALSE(view_->progress_bar_for_testing()->GetVisible());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ERROR_TITLE));
  EXPECT_NE(
      view_->GetSecondaryMessage(),
      l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_CLEANING_UP_MESSAGE));

  view_->CancelDialog();
  ASSERT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, InstallThenSuccess) {
  ShowUi("default");
  EXPECT_CALL(*installer_, Install);
  EXPECT_CALL(*installer_, Cancel).Times(0);

  view_->AcceptDialog();
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));
  auto first_message = view_->GetSecondaryMessage();

  // Check that state changes update the progress message.
  view_->StateChanged(bruschetta::BruschettaInstaller::State::kStartVm);
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));
  EXPECT_NE(first_message, view_->GetSecondaryMessage());

  view_->OnInstallationEnded();

  // We close the installer upon completion since we switch to a terminal
  // window to complete the install.
  ASSERT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BruschettaInstallerViewBrowserTest, InstallWithRetry) {
  ShowUi("default");
  base::RunLoop run_loop;
  EXPECT_CALL(*installer_, Install);
  EXPECT_CALL(*installer_, Cancel).Times(0);

  // Start installing
  view_->AcceptDialog();
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_TRUE(view_->progress_bar_for_testing()->GetVisible());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));

  // An error happened
  view_->Error(BruschettaInstallResult::kStartVmFailed);
  // Let the cleanup step complete.
  run_loop.RunUntilIdle();
  EXPECT_NE(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_FALSE(view_->progress_bar_for_testing()->GetVisible());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ERROR_TITLE));

  // Retry to start installing again, since the installer gets recreated we need
  // to set expectations again.
  EXPECT_CALL(*installer_, Install);

  view_->AcceptDialog();
  EXPECT_EQ(nullptr, view_->GetOkButton());
  EXPECT_NE(nullptr, view_->GetCancelButton());
  EXPECT_TRUE(view_->progress_bar_for_testing()->GetVisible());
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_INSTALLER_ONGOING_TITLE));
}

}  // namespace
}  // namespace bruschetta
