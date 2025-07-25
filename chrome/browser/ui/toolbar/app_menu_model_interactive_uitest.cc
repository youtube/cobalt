// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_model.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);
}  // namespace

class AppMenuModelInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuModelInteractiveTest() = default;
  ~AppMenuModelInteractiveTest() override = default;
  AppMenuModelInteractiveTest(const AppMenuModelInteractiveTest&) = delete;
  void operator=(const AppMenuModelInteractiveTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  auto CheckInconitoWindowOpened() {
    return Check(base::BindLambdaForTesting([]() {
      Browser* new_browser;
      if (BrowserList::GetIncognitoBrowserCount() == 1) {
        new_browser = BrowserList::GetInstance()->GetLastActive();
      } else {
        new_browser = ui_test_utils::WaitForBrowserToOpen();
      }
      return new_browser->profile()->IsIncognitoProfile();
    }));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, PerformanceNavigation) {
  RunTestSequence(InstrumentTab(kPrimaryTabPageElementId),
                  PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
                  SelectMenuItem(ToolsMenuModel::kPerformanceMenuItem),
                  WaitForWebContentsNavigation(
                      kPrimaryTabPageElementId,
                      GURL(chrome::kChromeUIPerformanceSettingsURL)));
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, IncognitoMenuItem) {
  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kIncognitoMenuItem),
                  CheckInconitoWindowOpened());
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, IncognitoAccelerator) {
  ui::Accelerator incognito_accelerator;
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_NEW_INCOGNITO_WINDOW, &incognito_accelerator);

  RunTestSequence(
      SendAccelerator(kToolbarAppMenuButtonElementId, incognito_accelerator),
      CheckInconitoWindowOpened());
}

class ExtensionsMenuModelInteractiveTest : public AppMenuModelInteractiveTest {
 public:
  explicit ExtensionsMenuModelInteractiveTest(bool enable_feature = true) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (enable_feature) {
      enabled_features = {features::kExtensionsMenuInAppMenu};
      disabled_features = {features::kChromeRefresh2023};
    } else {
      enabled_features = {};
      disabled_features = {features::kExtensionsMenuInAppMenu,
                           features::kChromeRefresh2023};
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~ExtensionsMenuModelInteractiveTest() override = default;
  ExtensionsMenuModelInteractiveTest(
      const ExtensionsMenuModelInteractiveTest&) = delete;
  void operator=(const ExtensionsMenuModelInteractiveTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

 protected:
  base::HistogramTester histograms;
};

class ExtensionsMenuModelPresenceTest
    : public ExtensionsMenuModelInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionsMenuModelPresenceTest()
      : ExtensionsMenuModelInteractiveTest(/*enable_feature=*/GetParam()) {}
  ~ExtensionsMenuModelPresenceTest() override = default;
  ExtensionsMenuModelPresenceTest(const ExtensionsMenuModelPresenceTest&) =
      delete;
  void operator=(const ExtensionsMenuModelPresenceTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensionsMenuModelPresenceTest,
    testing::Bool(),
    [](const testing::TestParamInfo<ExtensionsMenuModelPresenceTest::ParamType>&
           info) { return info.param ? "InRootAppMenu" : "NotInRootAppMenu"; });

// Test to confirm that the structure of the Extensions menu is present but that
// no histograms are logged since it isn't interacted with.
IN_PROC_BROWSER_TEST_P(ExtensionsMenuModelPresenceTest, MenuPresence) {
  if (features::IsExtensionMenuInRootAppMenu()) {  // Menu enabled
    RunTestSequence(
        InstrumentTab(kPrimaryTabPageElementId),
        PressButton(kToolbarAppMenuButtonElementId),
        EnsurePresent(AppMenuModel::kExtensionsMenuItem),
        SelectMenuItem(AppMenuModel::kExtensionsMenuItem),
        EnsurePresent(ExtensionsMenuModel::kManageExtensionsMenuItem),
        EnsurePresent(ExtensionsMenuModel::kVisitChromeWebStoreMenuItem));
  } else {
    RunTestSequence(InstrumentTab(kPrimaryTabPageElementId),
                    PressButton(kToolbarAppMenuButtonElementId),
                    EnsureNotPresent(AppMenuModel::kExtensionsMenuItem));
  }

  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.VisitChromeWebStore", 0);
  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.ManageExtensions", 0);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_MANAGE_EXTENSIONS, 0);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_VISIT_CHROME_WEB_STORE, 0);
}

// Test to confirm that the manage extensions menu item navigates when selected
// and emite histograms that it did so.
IN_PROC_BROWSER_TEST_F(ExtensionsMenuModelInteractiveTest, ManageExtensions) {
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kExtensionsMenuItem),
      SelectMenuItem(ExtensionsMenuModel::kManageExtensionsMenuItem),
      WaitForWebContentsNavigation(kPrimaryTabPageElementId,
                                   GURL(chrome::kChromeUIExtensionsURL)));

  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.ManageExtensions", 1);
  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.VisitChromeWebStore", 0);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_MANAGE_EXTENSIONS, 1);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_VISIT_CHROME_WEB_STORE, 0);
}

// TODO(crbug.com/1488136): Remove this test in favor of a unit test
// extension_urls::GetWebstoreLaunchURL().
class ExtensionsMenuVisitChromeWebstoreModelInteractiveTest
    : public AppMenuModelInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionsMenuVisitChromeWebstoreModelInteractiveTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kExtensionsMenuInAppMenu};
    std::vector<base::test::FeatureRef> disabled_features{};
    if (GetParam()) {
      enabled_features.push_back(extensions_features::kNewWebstoreURL);
    } else {
      LOG(ERROR) << "disabling new webstore URL";
      disabled_features.push_back(extensions_features::kNewWebstoreURL);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

 protected:
  base::HistogramTester histograms;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensionsMenuVisitChromeWebstoreModelInteractiveTest,
    // extensions_features::kNewWebstoreURL enabled status.
    testing::Bool(),
    [](const testing::TestParamInfo<ExtensionsMenuModelPresenceTest::ParamType>&
           info) {
      return info.param ? "NewVisitChromeWebstoreUrl"
                        : "OldVisitChromeWebstoreUrl";
    });

// Test to confirm that the visit Chrome Web Store menu item navigates to the
// correct chrome webstore URL when selected and emits histograms that it did
// so.
IN_PROC_BROWSER_TEST_P(ExtensionsMenuVisitChromeWebstoreModelInteractiveTest,
                       VisitChromeWebStore) {
  GURL expected_webstore_launch_url =
      GetParam() ? extension_urls::GetNewWebstoreLaunchURL()
                 : extension_urls::GetWebstoreLaunchURL();
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kExtensionsMenuItem),
      SelectMenuItem(ExtensionsMenuModel::kVisitChromeWebStoreMenuItem),
      WaitForWebContentsNavigation(
          kPrimaryTabPageElementId,
          extension_urls::AppendUtmSource(expected_webstore_launch_url,
                                          extension_urls::kAppMenuUtmSource)));

  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.VisitChromeWebStore", 1);
  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.ManageExtensions", 0);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_VISIT_CHROME_WEB_STORE, 1);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_MANAGE_EXTENSIONS, 0);
}

class PasswordManagerMenuItemInteractiveTest
    : public AppMenuModelInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  PasswordManagerMenuItemInteractiveTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kChromeRefresh2023, false},
         {features::kChromeRefreshSecondary2023, false}});
  }
  PasswordManagerMenuItemInteractiveTest(
      const PasswordManagerMenuItemInteractiveTest&) = delete;
  void operator=(const PasswordManagerMenuItemInteractiveTest&) = delete;

  ~PasswordManagerMenuItemInteractiveTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PasswordManagerMenuItemInteractiveTest,
                       PasswordManagerMenuItem) {
  base::HistogramTester histograms;

  RunTestSequence(InstrumentTab(kPrimaryTabPageElementId),
                  PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kPasswordManagerMenuItem),
                  WaitForWebContentsNavigation(
                      kPrimaryTabPageElementId,
                      GURL("chrome://password-manager/passwords")));

  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.PasswordManager", 1);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_PASSWORD_MANAGER, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerMenuItemInteractiveTest,
                       NoMenuItemOnPasswordManagerPage) {
  RunTestSequence(
      AddInstrumentedTab(kPrimaryTabPageElementId,
                         GURL("chrome://password-manager/passwords")),
      WaitForWebContentsReady(kPrimaryTabPageElementId,
                              GURL("chrome://password-manager/passwords")),
      PressButton(kToolbarAppMenuButtonElementId),
      EnsureNotPresent(AppMenuModel::kPasswordManagerMenuItem));
}
