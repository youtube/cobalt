// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Test both aliases during migration. See crbug.com/1328581.
constexpr char kOldPermissionName[] = "window-placement";
constexpr char kNewPermissionName[] = "window-management";

typedef std::tuple<bool, bool> PermissionTestParams;

class WindowManagementTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PermissionTestParams> {
 public:
  WindowManagementTest() {
    scoped_feature_list_.InitWithFeatureState(
        permissions::features::kWindowManagementPermissionAlias,
        AliasEnabled());
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Window management features are only available on secure contexts, and so
    // we need to create an HTTPS test server here to serve those pages rather
    // than using the default https_test_server_.
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    // Support sites like a.test, b.test, c.test etc
    https_test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_test_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    net::test_server::RegisterDefaultHandlers(https_test_server_.get());
    content::SetupCrossSiteRedirector(https_test_server_.get());
    ASSERT_TRUE(https_test_server_->Start());
  }

  void SetupTwoIframes() {
    const GURL url(
        https_test_server_->GetURL("a.test", "/two_iframes_blank.html"));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

    EXPECT_TRUE(ExecJs(
        tab,
        base::ReplaceStringPlaceholders(
            R"(
      const frame1 = document.getElementById('iframe1');
      frame1.setAttribute('allow', '$1');
      const frame2 = document.getElementById('iframe2');
      frame2.setAttribute('allow', '$1');)",
            {UseAlias() ? kNewPermissionName : kOldPermissionName}, nullptr),
        content::EXECUTE_SCRIPT_NO_USER_GESTURE));

    GURL subframe_url1(https_test_server_->GetURL("a.test", "/title1.html"));
    GURL subframe_url2(https_test_server_->GetURL("b.test", "/title1.html"));
    content::NavigateIframeToURL(tab, /*iframe_id=*/"iframe1", subframe_url1);
    content::NavigateIframeToURL(tab, /*iframe_id=*/"iframe2", subframe_url2);

    // TODO(crbug.com/1119974): this test could be in content_browsertests
    // and not browser_tests if permission controls were supported.

    // Auto-accept the Window Placement permission request.
    permissions::PermissionRequestManager* permission_request_manager_tab =
        permissions::PermissionRequestManager::FromWebContents(tab);
    permission_request_manager_tab->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);
  }

 protected:
  bool AliasEnabled() const { return std::get<0>(GetParam()); }
  bool UseAlias() const { return std::get<1>(GetParam()); }
  bool ShouldError() const { return UseAlias() && !AliasEnabled(); }

  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1183791): Disabled on non-ChromeOS because of races with
// SetScreenInstance and observers not being notified.
// TODO(crbug.com/1297812): Completely disabled as this test is also flaky on
// the CrOS bot.
IN_PROC_BROWSER_TEST_P(WindowManagementTest, DISABLED_OnScreensChangeEvent) {
  // Updates the display configuration to add a secondary display.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+1-801x802");
#else
  display::ScreenBase screen;
  screen.display_list().AddDisplay({1, gfx::Rect(100, 1, 801, 802)},
                                   display::DisplayList::Type::PRIMARY);
  display::Screen::SetScreenInstance(&screen);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  SetupTwoIframes();
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* local_child =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* remote_child =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 1);

  auto initial_result = content::ListValueOf(801);
  auto* initial_script = R"(
      var screenDetails;
      var promiseForEvent = (target, evt) => {
        return new Promise((resolve) => {
          const handler = (e) => {
            target.removeEventListener(evt, handler);
            resolve(e);
          };
          target.addEventListener(evt, handler);
        });
      }
      var makeScreensChangePromise = () => {
        if (!screenDetails) return undefined;
        return promiseForEvent(screenDetails, 'screenschange');
      };
      var getScreenWidths = () => {
        return screenDetails.screens.map((d) => d.width).sort();
      };

      (async () => {
          screenDetails = await self.getScreenDetails();
          return getScreenWidths();
      })();
  )";

  ASSERT_EQ(initial_result, EvalJs(tab, initial_script));
  ASSERT_EQ(initial_result, EvalJs(local_child, initial_script));
  // Remote frame should error when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_FALSE(EvalJs(remote_child, initial_script).error.empty());
  } else {
    ASSERT_EQ(initial_result, EvalJs(remote_child, initial_script));
  }

  // Add a second display.
  auto* add_screens_change_promise =
      R"(var screensChange = makeScreensChangePromise();)";
  EXPECT_TRUE(ExecJs(tab, add_screens_change_promise));
  EXPECT_TRUE(ExecJs(local_child, add_screens_change_promise));
  EXPECT_TRUE(ExecJs(remote_child, add_screens_change_promise));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x802");
#else
  screen.display_list().AddDisplay({2, gfx::Rect(901, 100, 802, 802)},
                                   display::DisplayList::Type::PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  auto* await_screens_change = R"(
      (async () => {
          if (!screensChange) return -1;
          await screensChange;
          return getScreenWidths();
      })();
  )";

  {
    auto result = content::ListValueOf(801, 802);

    EXPECT_EQ(result, EvalJs(tab, await_screens_change));
    EXPECT_EQ(result, EvalJs(local_child, await_screens_change));
    // screensChange is undefined for remote frame when alias is used but not
    // enabled.
    if (ShouldError()) {
      EXPECT_EQ(-1, EvalJs(remote_child, await_screens_change));
    } else {
      EXPECT_EQ(result, EvalJs(remote_child, await_screens_change));
    }
  }

  // Remove the first display.
  EXPECT_TRUE(ExecJs(tab, add_screens_change_promise));
  EXPECT_TRUE(ExecJs(local_child, add_screens_change_promise));
  EXPECT_TRUE(ExecJs(remote_child, add_screens_change_promise));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("901+100-802x802");
#else
  // Make the second display primary so we can remove the first.
  EXPECT_EQ(screen.display_list().displays().size(), 2u);
  screen.display_list().RemoveDisplay(1);
  EXPECT_EQ(screen.display_list().displays().size(), 1u);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  {
    auto result = content::ListValueOf(802);

    EXPECT_EQ(result, EvalJs(tab, await_screens_change));
    EXPECT_EQ(result, EvalJs(local_child, await_screens_change));
    // screensChange is undefined for remote frame when alias is used but not
    // enabled.
    if (ShouldError()) {
      EXPECT_EQ(-1, EvalJs(remote_child, await_screens_change));
    } else {
      EXPECT_EQ(result, EvalJs(remote_child, await_screens_change));
    }
  }

  // Remove one display, add two displays.
  // TODO(enne): we add two displays here because DisplayManagerTestApi
  // would consider remove+add to just be an update (with the same id).
  // An alternative is to modify DisplayManagerTestApi to let us set ids.
  EXPECT_TRUE(ExecJs(tab, add_screens_change_promise));
  EXPECT_TRUE(ExecJs(local_child, add_screens_change_promise));
  EXPECT_TRUE(ExecJs(remote_child, add_screens_change_promise));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("0+0-803x600,1000+0-804x600");
#else
  screen.display_list().RemoveDisplay(2);
  screen.display_list().AddDisplay({3, gfx::Rect(0, 4, 803, 600)},
                                   display::DisplayList::Type::PRIMARY);
  screen.display_list().AddDisplay({4, gfx::Rect(0, 4, 804, 600)},
                                   display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  {
    auto result = content::ListValueOf(803, 804);

    EXPECT_EQ(result, EvalJs(tab, await_screens_change));
    EXPECT_EQ(result, EvalJs(local_child, await_screens_change));
    // screensChange is undefined for remote frame when alias is used but not
    // enabled.
    if (ShouldError()) {
      EXPECT_EQ(-1, EvalJs(remote_child, await_screens_change));
    } else {
      EXPECT_EQ(result, EvalJs(remote_child, await_screens_change));
    }
  }
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  display::Screen::SetScreenInstance(nullptr);
#endif
}

// TODO(crbug.com/1183791): Disabled on non-ChromeOS because of races with
// SetScreenInstance and observers not being notified.
// TODO(crbug.com/1194700): Disabled on Mac because of GetScreenInfos staleness.
// TODO(crbug.com/1385598): Completely disabled as this test is also flaky on
// the CrOS bot.
// Test that the oncurrentscreenchange handler fires correctly for screen
// changes and property updates.  It also verifies that window.screen.onchange
// also fires in the same scenarios.  (This is not true in all cases, e.g.
// isInternal changing, but is true for width/height tests here.)
IN_PROC_BROWSER_TEST_F(WindowManagementTest,
                       DISABLED_OnCurrentScreenChangeEvent) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x802");
#else
  display::ScreenBase screen;
  screen.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                   display::DisplayList::Type::PRIMARY);
  screen.display_list().AddDisplay({2, gfx::Rect(901, 100, 802, 802)},
                                   display::DisplayList::Type::NOT_PRIMARY);
  display::Screen::SetScreenInstance(&screen);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  SetupTwoIframes();
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* local_child =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* remote_child =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 1);

  auto* initial_script = R"(
      var screenDetails;
      var promiseForEvent = (target, evt) => {
        return new Promise((resolve) => {
          const handler = (e) => {
            target.removeEventListener(evt, handler);
            resolve(e);
          };
          target.addEventListener(evt, handler);
        });
      }
      var makeCurrentScreenChangePromise = () => {
        if (!screenDetails) return undefined;
        return promiseForEvent(screenDetails, 'currentscreenchange');
      };
      var makeWindowScreenChangePromise = () => {
        return promiseForEvent(window.screen, 'change');
      };
      (async () => {
          screenDetails = await self.getScreenDetails();
          if (screenDetails.currentScreen.width != window.screen.width)
            return -1;
          return screenDetails.currentScreen.width;
      })();
  )";
  EXPECT_EQ(801, EvalJs(tab, initial_script));
  EXPECT_EQ(801, EvalJs(local_child, initial_script));
  // Remote frame should error when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_FALSE(EvalJs(remote_child, initial_script).error.empty());
  } else {
    EXPECT_EQ(801, EvalJs(remote_child, initial_script));
  }

  // Switch to a second display.  This should fire an event.
  auto* add_current_screen_change_promise = R"(
      var currentScreenChange = makeCurrentScreenChangePromise();
      var windowScreenChange = makeWindowScreenChangePromise();
  )";
  EXPECT_TRUE(ExecJs(tab, add_current_screen_change_promise));
  EXPECT_TRUE(ExecJs(local_child, add_current_screen_change_promise));
  EXPECT_TRUE(ExecJs(remote_child, add_current_screen_change_promise));

  const gfx::Rect new_bounds(1000, 150, 600, 500);
  browser()->window()->SetBounds(new_bounds);

  auto* await_change_width = R"(
      (async () => {
          if (!currentScreenChange || !windowScreenChange)
              return -2;
          await currentScreenChange;
          await windowScreenChange;
          if (screenDetails.currentScreen.width != window.screen.width)
            return -1;
          return screenDetails.currentScreen.width;
      })();
  )";
  EXPECT_EQ(802, EvalJs(tab, await_change_width));
  EXPECT_EQ(802, EvalJs(local_child, await_change_width));
  // currentScreenChange will be undefined when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_EQ(-2, EvalJs(remote_child, await_change_width));
  } else {
    EXPECT_EQ(802, EvalJs(remote_child, await_change_width));
  }

  // Update the second display to have a height of 300.  Validate that a change
  // event is fired when attributes of the current screen change.
  EXPECT_TRUE(ExecJs(tab, add_current_screen_change_promise));
  EXPECT_TRUE(ExecJs(local_child, add_current_screen_change_promise));
  EXPECT_TRUE(ExecJs(remote_child, add_current_screen_change_promise));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x300");
#else
  screen.display_list().UpdateDisplay({2, gfx::Rect(901, 100, 802, 300)},
                                      display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto* await_change_height = R"(
      (async () => {
          if (!currentScreenChange || !windowScreenChange)
              return -2;
          await currentScreenChange;
          await windowScreenChange;
          if (screenDetails.currentScreen.height != window.screen.height)
            return -1;
          return screenDetails.currentScreen.height;
      })();
  )";
  EXPECT_EQ(300, EvalJs(tab, await_change_height));
  EXPECT_EQ(300, EvalJs(local_child, await_change_height));
  // currentScreenChange will be undefined when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_EQ(-2, EvalJs(remote_child, await_change_height));
  } else {
    EXPECT_EQ(300, EvalJs(remote_child, await_change_height));
  }
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  display::Screen::SetScreenInstance(nullptr);
#endif
}

// TODO(crbug.com/1183791): Disabled on non-ChromeOS because of races with
// SetScreenInstance and observers not being notified.
// TODO(crbug.com/1194700): Disabled on Mac because of GetScreenInfos staleness.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_ScreenDetailedOnChange DISABLED_ScreenDetailedOnChange
#else
#define MAYBE_ScreenDetailedOnChange ScreenDetailedOnChange
#endif
// Test that onchange events for individual screens in the screen list are
// supported.
IN_PROC_BROWSER_TEST_P(WindowManagementTest, MAYBE_ScreenDetailedOnChange) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x802,901+100-802x802");
#else
  display::ScreenBase screen;
  screen.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                   display::DisplayList::Type::PRIMARY);
  screen.display_list().AddDisplay({2, gfx::Rect(901, 100, 802, 802)},
                                   display::DisplayList::Type::NOT_PRIMARY);
  display::Screen::SetScreenInstance(&screen);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  SetupTwoIframes();
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* local_child =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* remote_child =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 1);

  auto* initial_script = R"(
      var screenDetails;
      var promiseForEvent = (target, evt) => {
        return new Promise((resolve) => {
          const handler = (e) => {
            target.removeEventListener(evt, handler);
            resolve(e);
          };
          target.addEventListener(evt, handler);
        });
      }
      var screenChanges0 = 0;
      var screenChanges1 = 0;
      (async () => {
        screenDetails = await self.getScreenDetails();
        if (screenDetails.screens.length !== 2)
          return false;
        // Add some event listeners for individual screens.
        screenDetails.screens[0].addEventListener('change', () => {
          screenChanges0++;
        });
        screenDetails.screens[1].addEventListener('change', () => {
          screenChanges1++;
        });
        return true;
      })();
  )";
  EXPECT_EQ(true, EvalJs(tab, initial_script));
  EXPECT_EQ(true, EvalJs(local_child, initial_script));
  // Remote frame should error when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_FALSE(EvalJs(remote_child, initial_script).error.empty());
  } else {
    EXPECT_EQ(true, EvalJs(remote_child, initial_script));
  }

  // Update only the first display to have a different height.
  auto* add_change0 = R"(
    var change0 = promiseForEvent(screenDetails.screens[0], 'change');
  )";
  EXPECT_TRUE(ExecJs(tab, add_change0));
  EXPECT_TRUE(ExecJs(local_child, add_change0));
  // Remote frame should error when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_FALSE(EvalJs(remote_child, add_change0).error.empty());
  } else {
    EXPECT_TRUE(ExecJs(remote_child, add_change0));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x301,901+100-802x802");
#else
  screen.display_list().UpdateDisplay({1, gfx::Rect(100, 100, 801, 301)},
                                      display::DisplayList::Type::PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto* await_change0_height = R"(
      (async () => {
          if (!change0) return -3;
          await change0;
          // Only screen[0] should have changed.
          if (screenChanges0 !== 1)
            return -1;
          if (screenChanges1 !== 0)
            return -2;
          return screenDetails.screens[0].height;
      })();
  )";
  EXPECT_EQ(301, EvalJs(tab, await_change0_height));
  EXPECT_EQ(301, EvalJs(local_child, await_change0_height));
  // change0 is undefined when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_EQ(-3, EvalJs(remote_child, await_change0_height));
  } else {
    EXPECT_EQ(301, EvalJs(remote_child, await_change0_height));
  }

  // Update only the second display to have a different height.
  auto* add_change1 = R"(
    var change1 = promiseForEvent(screenDetails.screens[1], 'change');
  )";
  EXPECT_TRUE(ExecJs(tab, add_change1));
  EXPECT_TRUE(ExecJs(local_child, add_change1));
  // Remote frame should error when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_FALSE(EvalJs(remote_child, add_change1).error.empty());
  } else {
    EXPECT_TRUE(ExecJs(remote_child, add_change1));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-801x301,901+100-802x302");
#else
  screen.display_list().UpdateDisplay({2, gfx::Rect(901, 100, 802, 302)},
                                      display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto* await_change1_height = R"(
      (async () => {
          if (!change1)
            return -3;
          await change1;
          // Both screens have one change.
          if (screenChanges0 !== 1)
            return -1;
          if (screenChanges1 !== 1)
            return -2;
          return screenDetails.screens[1].height;
      })();
  )";
  EXPECT_EQ(302, EvalJs(tab, await_change1_height));
  EXPECT_EQ(302, EvalJs(local_child, await_change1_height));
  // change1 is undefined when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_EQ(-3, EvalJs(remote_child, await_change1_height));
  } else {
    EXPECT_EQ(302, EvalJs(remote_child, await_change1_height));
  }

  // Change the width of both displays at the same time.
  auto* add_both_changes = R"(
    var change0 = promiseForEvent(screenDetails.screens[0], 'change');
    var change1 = promiseForEvent(screenDetails.screens[1], 'change');
  )";
  EXPECT_TRUE(ExecJs(tab, add_both_changes));
  EXPECT_TRUE(ExecJs(local_child, add_both_changes));
  // Remote frame should error when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_FALSE(EvalJs(remote_child, add_both_changes).error.empty());
  } else {
    EXPECT_TRUE(ExecJs(remote_child, add_both_changes));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay("100+100-401x301,901+100-402x302");
#else
  screen.display_list().UpdateDisplay({1, gfx::Rect(100, 100, 401, 301)},
                                      display::DisplayList::Type::PRIMARY);
  screen.display_list().UpdateDisplay({2, gfx::Rect(901, 100, 402, 302)},
                                      display::DisplayList::Type::NOT_PRIMARY);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto* await_both_changes_width = R"(
      (async () => {
          if (!change0 || !change1)
            return -3;
          await change0;
          await change1;
          // Both screens have two changes
          if (screenChanges0 !== 2)
            return false;
          if (screenChanges1 !== 2)
            return false;
          if (screenDetails.screens[0].width !== 401)
            return false;
          if (screenDetails.screens[1].width !== 402)
            return false;
          return true;
      })();
  )";
  EXPECT_EQ(true, EvalJs(tab, await_both_changes_width));
  EXPECT_EQ(true, EvalJs(local_child, await_both_changes_width));
  // change1 and change2 are undefined when alias used but not enabled.
  if (ShouldError()) {
    EXPECT_EQ(-3, EvalJs(remote_child, await_both_changes_width));
  } else {
    EXPECT_EQ(true, EvalJs(remote_child, await_both_changes_width));
  }
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  display::Screen::SetScreenInstance(nullptr);
#endif
}

INSTANTIATE_TEST_SUITE_P(,
                         WindowManagementTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));
