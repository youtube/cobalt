// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <memory>

#include "chrome/browser/ash/android_sms/fake_android_sms_app_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#endif

class PushMessagingNotificationManagerTest
    : public ChromeRenderViewHostTestHarness {};

TEST_F(PushMessagingNotificationManagerTest, IsTabVisible) {
  PushMessagingNotificationManager manager(profile());
  GURL origin("https://google.com/");
  GURL origin_with_path = origin.Resolve("/path/");
  NavigateAndCommit(origin_with_path);

  EXPECT_FALSE(manager.IsTabVisible(profile(), nullptr, origin));
  EXPECT_FALSE(manager.IsTabVisible(profile(), web_contents(),
                                    GURL("https://chrome.com/")));
  EXPECT_TRUE(manager.IsTabVisible(profile(), web_contents(), origin));

  content::RenderViewHostTester::For(rvh())->SimulateWasHidden();
  EXPECT_FALSE(manager.IsTabVisible(profile(), web_contents(), origin));

  content::RenderViewHostTester::For(rvh())->SimulateWasShown();
  EXPECT_TRUE(manager.IsTabVisible(profile(), web_contents(), origin));
}

TEST_F(PushMessagingNotificationManagerTest, IsTabVisibleViewSource) {
  PushMessagingNotificationManager manager(profile());

  GURL origin("https://google.com/");
  GURL view_source_page("view-source:https://google.com/path/");

  NavigateAndCommit(view_source_page);

  ASSERT_EQ(view_source_page, web_contents()->GetVisibleURL());
  EXPECT_TRUE(manager.IsTabVisible(profile(), web_contents(), origin));

  content::RenderViewHostTester::For(rvh())->SimulateWasHidden();
  EXPECT_FALSE(manager.IsTabVisible(profile(), web_contents(), origin));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PushMessagingNotificationManagerTest,
       SkipEnforceUserVisibleOnlyRequirementsForAndroidMessages) {
  GURL app_url("https://example.com/test/");
  auto fake_android_sms_app_manager =
      std::make_unique<ash::android_sms::FakeAndroidSmsAppManager>();
  fake_android_sms_app_manager->SetInstalledAppUrl(app_url);

  auto fake_multidevice_setup_client =
      std::make_unique<ash::multidevice_setup::FakeMultiDeviceSetupClient>();
  fake_multidevice_setup_client->SetFeatureState(
      ash::multidevice_setup::mojom::Feature::kMessages,
      ash::multidevice_setup::mojom::FeatureState::kEnabledByUser);

  PushMessagingNotificationManager manager(profile());
  manager.SetTestMultiDeviceSetupClient(fake_multidevice_setup_client.get());
  manager.SetTestAndroidSmsAppManager(fake_android_sms_app_manager.get());

  bool was_called = false;
  manager.EnforceUserVisibleOnlyRequirements(
      app_url.DeprecatedGetOriginAsURL(), 0l,
      base::BindOnce(
          [](bool* was_called, bool did_show_generic_notification) {
            *was_called = true;
          },
          &was_called));
  EXPECT_TRUE(was_called);
}
#endif
