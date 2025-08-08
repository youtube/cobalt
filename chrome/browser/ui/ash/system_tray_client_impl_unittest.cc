// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "url/gurl.h"

namespace {

constexpr const char kShowTouchpadSettingsPage[] = "ShowTouchpadSettingsPage";
constexpr const char kShowRemapKeysSettingsSubpage[] =
    "ShowRemapKeysSettingsSubpage";

class TestSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  void ShowChromePageForProfile(Profile* profile,
                                const GURL& gurl,
                                int64_t display_id) override {
    last_url_ = gurl;
  }

  const GURL& last_url() { return last_url_; }

 private:
  GURL last_url_;
};

// Use BrowserWithTestWindowTest because it sets up ash::Shell, ash::SystemTray,
// ProfileManager, etc.
class SystemTrayClientImplTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    client_impl_ = std::make_unique<SystemTrayClientImpl>();
    settings_window_manager_ = std::make_unique<TestSettingsWindowManager>();

    chrome::SettingsWindowManager::SetInstanceForTesting(
        settings_window_manager_.get());
  }
  void TearDown() override {
    chrome::SettingsWindowManager::SetInstanceForTesting(nullptr);
    settings_window_manager_.reset();
    client_impl_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  std::unique_ptr<SystemTrayClientImpl> client_impl_;
  std::unique_ptr<TestSettingsWindowManager> settings_window_manager_;
};

TEST_F(SystemTrayClientImplTest, ShowAccountSettings) {
  client_impl_->ShowAccountSettings();
  EXPECT_EQ(
      settings_window_manager_->last_url(),
      chrome::GetOSSettingsUrl(chromeos::settings::mojom::kPeopleSectionPath));
}

TEST_F(SystemTrayClientImplTest, ShowTouchpadSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kInputDeviceSettingsSplit);
  base::UserActionTester user_action_tester;
  client_impl_->ShowTouchpadSettings();
  EXPECT_EQ(settings_window_manager_->last_url(),
            chrome::GetOSSettingsUrl(
                chromeos::settings::mojom::kPerDeviceTouchpadSubpagePath));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kShowTouchpadSettingsPage));
}

TEST_F(SystemTrayClientImplTest, ShowRemapKeysSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kInputDeviceSettingsSplit);
  base::UserActionTester user_action_tester;
  client_impl_->ShowRemapKeysSubpage(/*device_id=*/1);
  EXPECT_EQ(settings_window_manager_->last_url(),
            base::StrCat({chrome::GetOSSettingsUrl(
                              chromeos::settings::mojom::
                                  kPerDeviceKeyboardRemapKeysSubpagePath)
                              .spec(),
                          "?keyboardId=1"}));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(kShowRemapKeysSettingsSubpage));
}

TEST_F(SystemTrayClientImplTest, ShowApnSubpage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kApnRevamp);
  client_impl_->ShowApnSubpage(/*network_id=*/"guid");
  EXPECT_EQ(settings_window_manager_->last_url(),
            base::StrCat({chrome::GetOSSettingsUrl(
                              chromeos::settings::mojom::kApnSubpagePath)
                              .spec(),
                          "?guid=guid"}));
}

}  // namespace
