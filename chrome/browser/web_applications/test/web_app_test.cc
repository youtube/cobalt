// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/test/debug_info_printer.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

WebAppTest::~WebAppTest() = default;

void WebAppTest::SetUp() {
#if BUILDFLAG(IS_WIN)
  registry_override_.OverrideRegistry(HKEY_CURRENT_USER);
#endif  // BUILDFLAG(IS_WIN)
  ASSERT_TRUE(testing_profile_manager_.SetUp());
  profile_ = testing_profile_manager_.CreateTestingProfile(
      TestingProfile::kDefaultProfileUserName, /*is_main_profile=*/true,
      shared_url_loader_factory_);
  content::RenderViewHostTestHarness::SetUp();
}

void WebAppTest::TearDown() {
  if (testing::Test::HasFailure()) {
    base::TimeDelta log_time = base::TimeTicks::Now() - start_time_;
    web_app::test::LogDebugInfoToConsole(
        testing_profile_manager_.profile_manager()->GetLoadedProfiles(),
        log_time);
  }
  // RenderViewHostTestHarness::TearDown destroys the TaskEnvironment. We need
  // to destroy profiles before that happens, and web contents need to be
  // destroyed before profiles are destroyed.
  DeleteContents();
  // Make sure that we flush any messages related to WebContentsImpl
  // destruction before we destroy the profiles.
  base::RunLoop().RunUntilIdle();
  testing_profile_manager_.DeleteAllTestingProfiles();
  content::RenderViewHostTestHarness::TearDown();
}

content::BrowserContext* WebAppTest::GetBrowserContext() {
  return profile();
}

web_app::FakeWebAppProvider& WebAppTest::fake_provider() {
  return *web_app::FakeWebAppProvider::Get(profile());
}
