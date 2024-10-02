// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/display/mac/test/virtual_display_mac_util.h"
#endif

namespace web_app {
namespace {
constexpr const char kExampleURL[] = "http://example.org/";
}

class WebAppInteractiveUiTest : public WebAppControllerBrowserTest {};

// Disabled everywhere except ChromeOS and Mac because those are the only
// platforms with functional display mocking at the moment. While a partial
// solution is possible using display::Screen::SetScreenInstance on other
// platforms, window placement doesn't work right with a faked Screen
// instance.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_TabOpensOnCorrectDisplay TabOpensOnCorrectDisplay
#else
#define MAYBE_TabOpensOnCorrectDisplay DISABLED_TabOpensOnCorrectDisplay
#endif
// Tests that PWAs that open in a tab open tabs on the correct display.
IN_PROC_BROWSER_TEST_F(WebAppInteractiveUiTest,
                       MAYBE_TabOpensOnCorrectDisplay) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("801x802,802x802");
#elif BUILDFLAG(IS_MAC)
  if (!display::test::VirtualDisplayMacUtil::IsAPIAvailable()) {
    GTEST_SKIP() << "Skipping test for unsupported MacOS version.";
  }
  display::test::VirtualDisplayMacUtil virtual_display_mac_util;
  virtual_display_mac_util.AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Install test app.
  const AppId app_id = InstallPWA(GURL(kExampleURL));

  // Figure out what display the original tabbed browser was created on, as well
  // as what the display Id is for a second display.
  Browser* original_browser = browser();
  const std::vector<display::Display>& displays =
      display::Screen::GetScreen()->GetAllDisplays();
  display::Display original_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          original_browser->window()->GetNativeWindow());
  display::Display other_display;
  for (const auto& d : displays) {
    if (d.id() != original_display.id()) {
      other_display = d;
      break;
    }
  }
  ASSERT_TRUE(other_display.is_valid());

  // By default opening a PWA in a tab should open on the same display as the
  // existing tabbed browser, and thus in the same browser window.
  Browser* app_browser = LaunchBrowserForWebAppInTab(app_id);
  EXPECT_EQ(app_browser, original_browser);
  EXPECT_EQ(2, original_browser->tab_strip_model()->count());

  {
    // Setting the display for new windows only effects what display a browser
    // window is opened on on Chrome OS. On other platforms a new tab should
    // be opened in the existing window regardless of what display we set for
    // new windows.
    display::ScopedDisplayForNewWindows scoped_display(other_display.id());
    app_browser = LaunchBrowserForWebAppInTab(app_id);

#if BUILDFLAG(IS_CHROMEOS)
    EXPECT_NE(app_browser, original_browser);
    EXPECT_EQ(
        other_display.id(),
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(app_browser->window()->GetNativeWindow())
            .id());
    EXPECT_EQ(2, original_browser->tab_strip_model()->count());
    EXPECT_EQ(1, app_browser->tab_strip_model()->count());

    // A second launch should re-use the same browser window.
    Browser* app_browser2 = LaunchBrowserForWebAppInTab(app_id);
    EXPECT_EQ(app_browser, app_browser2);
    EXPECT_EQ(2, original_browser->tab_strip_model()->count());
    EXPECT_EQ(2, app_browser->tab_strip_model()->count());
#else
    EXPECT_EQ(app_browser, original_browser);
    EXPECT_EQ(3, original_browser->tab_strip_model()->count());
#endif
  }

  {
    // Forcing the app to launch on the original display should open a new tab
    // in the original browser.
    display::ScopedDisplayForNewWindows scoped_display(original_display.id());
    app_browser = LaunchBrowserForWebAppInTab(app_id);
    EXPECT_EQ(app_browser, original_browser);
#if BUILDFLAG(IS_CHROMEOS)
    EXPECT_EQ(3, original_browser->tab_strip_model()->count());
#else
    EXPECT_EQ(4, original_browser->tab_strip_model()->count());
#endif
  }
}

}  // namespace web_app
