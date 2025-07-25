// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/features/feature_channel.h"

namespace ash {

class AccessibilityCommonTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<version_info::Channel> {
 public:
  bool DoesComponentExtensionExist(const std::string& id) {
    return extensions::ExtensionSystem::Get(
               AccessibilityManager::Get()->profile())
        ->extension_service()
        ->component_loader()
        ->Exists(id);
  }

  void SetUpOnMainThread() override {
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);
  }

  void TearDownOnMainThread() override {
    EXPECT_FALSE(console_observer_->HasErrorsOrWarnings())
        << "Found console.warn or console.error with message: "
        << console_observer_->GetErrorOrWarningAt(0);
  }

 protected:
  AccessibilityCommonTest() = default;

 private:
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
};

IN_PROC_BROWSER_TEST_P(AccessibilityCommonTest, ToggleFeatures) {
  extensions::ScopedCurrentChannel channel(GetParam());

  AccessibilityManager* manager = AccessibilityManager::Get();
  const auto& enabled_features =
      manager->GetAccessibilityCommonEnabledFeaturesForTest();

  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  PrefService* pref_service = manager->profile()->GetPrefs();

  {
    extensions::ExtensionHostTestHelper host_helper(
        manager->profile(), extension_misc::kAccessibilityCommonExtensionId);
    pref_service->SetBoolean(prefs::kAccessibilityAutoclickEnabled, true);
    host_helper.WaitForHostCompletedFirstLoad();
  }

  EXPECT_EQ(1U, enabled_features.size());
  EXPECT_EQ(1U, enabled_features.count(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, true);
  EXPECT_EQ(2U, enabled_features.size());
  EXPECT_EQ(1U, enabled_features.count(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_EQ(
      1U, enabled_features.count(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(prefs::kAccessibilityAutoclickEnabled, false);
  EXPECT_EQ(1U, enabled_features.size());
  EXPECT_EQ(
      1U, enabled_features.count(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  pref_service->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled, false);
  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));

  {
    extensions::ExtensionHostTestHelper host_helper(
        manager->profile(), extension_misc::kChromeVoxExtensionId);
    // Not an accessibility common feature.
    pref_service->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, true);
    host_helper.WaitForHostCompletedFirstLoad();
  }

  EXPECT_TRUE(enabled_features.empty());
  EXPECT_FALSE(DoesComponentExtensionExist(
      extension_misc::kAccessibilityCommonExtensionId));
  EXPECT_TRUE(
      DoesComponentExtensionExist(extension_misc::kChromeVoxExtensionId));
}

INSTANTIATE_TEST_SUITE_P(AllChannels,
                         AccessibilityCommonTest,
                         testing::Values(version_info::Channel::STABLE,
                                         version_info::Channel::BETA,
                                         version_info::Channel::DEV,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEFAULT));

}  // namespace ash
