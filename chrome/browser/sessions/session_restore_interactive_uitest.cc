// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/sessions/content/content_test_helper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"

class SessionRestoreInteractiveTest : public InProcessBrowserTest {
 public:
  SessionRestoreInteractiveTest() = default;
  ~SessionRestoreInteractiveTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
  }

  bool SetUpUserDataDirectory() override {
    url1_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot1.html"));

    return InProcessBrowserTest::SetUpUserDataDirectory();
  }

  Browser* QuitBrowserAndRestore(Browser* browser) {
    Profile* profile = browser->profile();

    // Close the browser.
    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
    CloseBrowserSynchronously(browser);

    ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
    SessionRestoreTestHelper restore_observer;

    // Ensure the session service factory is started, even if it was explicitly
    // shut down.
    SessionServiceTestHelper helper(profile);
    helper.SetForceBrowserNotAliveWithNoWindows(true);

    // Create a new window, which should trigger session restore.
    chrome::NewEmptyWindow(profile);

    Browser* new_browser =
        chrome::FindBrowserWithWebContents(tab_waiter.Wait());

    restore_observer.Wait();
    WaitForTabsToLoad(new_browser);

    keep_alive.reset();
    profile_keep_alive.reset();

    return new_browser;
  }

  void QuitMultiWindowBrowserAndRestore(Profile* profile) {
    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);

    int normal_window_counter = 0;
    int minimized_window_counter = 0;

    // Pretend to "close the browser."
    SessionServiceFactory::ShutdownForProfile(profile);
    while (Browser* browser = BrowserList::GetInstance()->GetLastActive()) {
      if (browser->window()->IsMinimized()) {
        minimized_window_counter++;
      } else {
        normal_window_counter++;
      }
      CloseBrowserSynchronously(browser);
    }

    // Now trigger a restore. We need to start up the services again
    // before restoring.
    SessionServiceFactory::GetForProfileForSessionRestore(profile);

    SessionRestore::RestoreSession(
        profile, nullptr,
        SessionRestore::SYNCHRONOUS | SessionRestore::RESTORE_BROWSER, {});

    for (Browser* browser : *(BrowserList::GetInstance())) {
      WaitForTabsToLoad(browser);
      if (browser->window()->IsMinimized()) {
        minimized_window_counter--;
      } else {
        normal_window_counter--;
      }
    }

    keep_alive.reset();
    profile_keep_alive.reset();

    EXPECT_EQ(0, normal_window_counter);
    EXPECT_EQ(0, minimized_window_counter);
  }

  void WaitForTabsToLoad(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      EXPECT_TRUE(content::WaitForLoadStop(contents));
    }
  }

  GURL url1_;
};

// TODO(https://crbug.com/1152160): Enable FocusOnLaunch on Lacros builds.
// TODO(https://crbug.com/1284590): Flaky on Linux ASAN/TSAN builders.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || \
    (BUILDFLAG(IS_LINUX) &&          \
     (defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)))
#define MAYBE_FocusOnLaunch DISABLED_FocusOnLaunch
#else
#define MAYBE_FocusOnLaunch FocusOnLaunch
#endif
IN_PROC_BROWSER_TEST_F(SessionRestoreInteractiveTest, MAYBE_FocusOnLaunch) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1_));

  Browser* new_browser = QuitBrowserAndRestore(browser());
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(url1_,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  ui_test_utils::BrowserActivationWaiter waiter(new_browser);
  waiter.WaitForActivation();

  // Ensure window has initial focus on launch.
  EXPECT_TRUE(new_browser->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetRenderWidgetHostView()
                  ->HasFocus());
}

// TODO(https://crbug.com/1152160): Enable RestoreMinimizedWindow on Lacros
// builds.
// TODO(crbug.com/1291651): Flaky failures.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_RestoreMinimizedWindow DISABLED_RestoreMinimizedWindow
#else
#define MAYBE_RestoreMinimizedWindow RestoreMinimizedWindow
#endif
// Verify that restoring a minimized window does not create a blank window.
// Regression test for https://crbug.com/1018885.
IN_PROC_BROWSER_TEST_F(SessionRestoreInteractiveTest,
                       MAYBE_RestoreMinimizedWindow) {
  // Minimize the window.
  browser()->window()->Minimize();

  // Restart and session restore the tabs.
  Browser* restored = QuitBrowserAndRestore(browser());
  EXPECT_EQ(1, restored->tab_strip_model()->count());

  // Expect the window to be visible.
  // Prior to the fix for https://crbug.com/1018885, the window was active but
  // not visible.
  EXPECT_TRUE(restored->window()->IsActive());
  EXPECT_TRUE(restored->window()->IsVisible());
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_RestoreMinimizedWindowTwice RestoreMinimizedWindowTwice
#else
#define MAYBE_RestoreMinimizedWindowTwice DISABLED_RestoreMinimizedWindowTwice
#endif
// Verify that in restoring a browser with a normal and minimized window twice,
// the minimized window remains minimized. Guards against a regression
// introduced in the fix for https://crbug.com/1204517. This test fails on
// Linux and Windows - see https://crbug.com/1213497.
IN_PROC_BROWSER_TEST_F(SessionRestoreInteractiveTest,
                       MAYBE_RestoreMinimizedWindowTwice) {
  Profile* profile = browser()->profile();

  // Create a second browser.
  CreateBrowser(browser()->profile());

  // Minimize the first browser window.
  browser()->window()->Minimize();

  EXPECT_EQ(2u, BrowserList::GetInstance()->size());

  // Quit and restore.
  QuitMultiWindowBrowserAndRestore(profile);

  // Quit and restore a second time.
  QuitMultiWindowBrowserAndRestore(profile);
}
