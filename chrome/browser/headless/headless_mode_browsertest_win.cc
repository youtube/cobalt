// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_browsertest.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace headless {

namespace {

// A class to expose a protected method for testing purposes.
class DesktopWindowTreeHostWinWrapper : public views::DesktopWindowTreeHostWin {
 public:
  HWND GetHWND() const { return DesktopWindowTreeHostWin::GetHWND(); }
};

INSTANTIATE_TEST_SUITE_P(HeadlessModeBrowserTestWithStartWindowMode,
                         HeadlessModeBrowserTestWithStartWindowMode,
                         testing::Values(kStartWindowNormal,
                                         kStartWindowMaximized,
                                         kStartWindowFullscreen));

IN_PROC_BROWSER_TEST_P(HeadlessModeBrowserTestWithStartWindowMode,
                       BrowserDesktopWindowHidden) {
  // On Windows, the Native Headless Chrome browser window exists and is
  // visible, while the underlying platform window is hidden.
  EXPECT_TRUE(browser()->window()->IsVisible());

  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_tree_host->GetHWND()));
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       ToggleFullscreenWindowVisibility) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify fullscreen state.
  ToggleFullscreenModeSync(browser());
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify back to normal state.
  ToggleFullscreenModeSync(browser());
  ASSERT_FALSE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       MinimizedRestoredWindowVisibility) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify minimized state.
  browser()->window()->Minimize();
  ASSERT_TRUE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify restored state.
  browser()->window()->Restore();
  ASSERT_FALSE(browser()->window()->IsMinimized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest,
                       MaximizedRestoredWindowVisibility) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Verify initial state.
  ASSERT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify maximized state.
  browser()->window()->Maximize();
  ASSERT_TRUE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));

  // Verify restored state.
  browser()->window()->Restore();
  ASSERT_FALSE(browser()->window()->IsMaximized());
  EXPECT_TRUE(browser()->window()->IsVisible());
  EXPECT_FALSE(::IsWindowVisible(desktop_window_hwnd));
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithWindowSize, LargeWindowSize) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Expect the platform window size to be smaller than the requested window
  // size due to Windows clamping the window dimensions to the monitor work
  // area.
  RECT platform_window_rect;
  CHECK(::GetWindowRect(desktop_window_hwnd, &platform_window_rect));
  EXPECT_LT(gfx::Rect(platform_window_rect).width(), kWindowSize.width());
  EXPECT_LT(gfx::Rect(platform_window_rect).height(), kWindowSize.height());

  // Expect the reported browser window size to be the same as the requested
  // window size.
  gfx::Rect bounds = browser()->window()->GetBounds();
  EXPECT_EQ(bounds.size(), kWindowSize);
}

IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTestWithWindowSizeAndScale,
                       WindowSizeWithScale) {
  DesktopWindowTreeHostWinWrapper* desktop_window_tree_host =
      static_cast<DesktopWindowTreeHostWinWrapper*>(
          browser()->window()->GetNativeWindow()->GetHost());
  HWND desktop_window_hwnd = desktop_window_tree_host->GetHWND();

  // Expect the platform window size to be larger than the requested window size
  // due to scaling.
  RECT platform_window_rect;
  CHECK(::GetWindowRect(desktop_window_hwnd, &platform_window_rect));
  EXPECT_GT(gfx::Rect(platform_window_rect).width(), kWindowSize.width());
  EXPECT_GT(gfx::Rect(platform_window_rect).height(), kWindowSize.height());

  // Expect the reported browser window size to be the same as the requested
  // window size.
  gfx::Rect bounds = browser()->window()->GetBounds();
  EXPECT_EQ(bounds.size(), kWindowSize);
}

}  // namespace

}  // namespace headless
