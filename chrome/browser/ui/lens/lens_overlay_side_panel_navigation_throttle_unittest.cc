// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_side_panel_navigation_throttle.h"

#include "base/logging.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "url/gurl.h"

namespace lens {
namespace {
constexpr char kValidSearchUrl[] =
    "https://www.google.com/search?q=text&gsc=1&masfc=c";
}  // namespace

class LensOverlaySidePanelNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }
};

TEST_F(LensOverlaySidePanelNavigationThrottleTest,
       MaybeCreateThrottle_ChildNavigationFailsIfSidePanelClosed) {
  auto* tester = content::RenderFrameHostTester::For(main_rfh());
  auto* child_frame = tester->AppendChild("results_frame");
  content::MockNavigationHandle handle(GURL(kValidSearchUrl), child_frame);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile());
  LensOverlaySidePanelNavigationThrottle::MaybeCreateAndAdd(registry,
                                                            theme_service);
  EXPECT_EQ(0u, registry.throttles().size());
}

TEST_F(LensOverlaySidePanelNavigationThrottleTest,
       MaybeCreateThrottle_TopLevelNavigationFails) {
  content::MockNavigationHandle handle(GURL(kValidSearchUrl), main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile());
  LensOverlaySidePanelNavigationThrottle::MaybeCreateAndAdd(registry,
                                                            theme_service);
  EXPECT_EQ(0u, registry.throttles().size());
}

}  // namespace lens
