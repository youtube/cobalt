// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "ash/public/cpp/debug_utils.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

namespace ash {

using DebugUtilsTest = AshTestBase;

TEST_F(DebugUtilsTest, PrintWindowHierarchy) {
  // Create 2 root windows by configuring two displays.
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Create w1 in first root window.
  std::unique_ptr<aura::Window> w1 =
      CreateToplevelTestWindow(gfx::Rect(0, 0, 100, 100));
  w1->SetTitle(u"Window1");

  // Create w2 in second root window.
  std::unique_ptr<aura::Window> w2 =
      CreateToplevelTestWindow(gfx::Rect(800, 0, 100, 100));
  w2->SetTitle(u"Window2");

  // Ensure w2 is active.
  wm::ActivateWindow(w2.get());
  ASSERT_EQ(w2.get(), window_util::GetActiveWindow());

  // Case 1: Mouse button NOT pressed.
  // Should print hierarchy for all root windows (both Window1 and Window2).
  {
    std::ostringstream out;
    std::vector<std::string> titles = debug::PrintWindowHierarchy(&out, false);
    EXPECT_TRUE(std::ranges::contains(titles, "Window1"));
    EXPECT_TRUE(std::ranges::contains(titles, "Window2"));
  }

  // Case 2: Mouse button IS pressed.
  // Should only print hierarchy for window under mouse (which is Window1).
  {
    auto* generator = GetEventGenerator();
    generator->MoveMouseToCenterOf(w1.get());
    generator->PressLeftButton();
    ASSERT_TRUE(aura::Env::GetInstance()->IsMouseButtonDown());

    std::ostringstream out;
    std::vector<std::string> titles = debug::PrintWindowHierarchy(&out, false);
    EXPECT_TRUE(std::ranges::contains(titles, "Window1"));
    EXPECT_FALSE(std::ranges::contains(titles, "Window2"));
    generator->ReleaseLeftButton();
  }

  // Case 3: Mouse button IS pressed, but mouse is over wallpaper (no window).
  // Should fallback to default behavior (print both Window1 and Window2)
  // and print a warning message.
  {
    auto* generator = GetEventGenerator();
    // Move mouse to (200, 200) where there is no window.
    generator->MoveMouseTo(gfx::Point(200, 200));
    generator->PressLeftButton();
    ASSERT_TRUE(aura::Env::GetInstance()->IsMouseButtonDown());

    std::ostringstream out;
    std::vector<std::string> titles = debug::PrintWindowHierarchy(&out, false);
    EXPECT_TRUE(std::ranges::contains(titles, "Window1"));
    EXPECT_TRUE(std::ranges::contains(titles, "Window2"));
    EXPECT_TRUE(
        out.str().find(
            "Warning: No window under mouse, falling back to default.") !=
        std::string::npos);
    generator->ReleaseLeftButton();
  }

  // Case 4: Mouse button IS pressed, and mouse is over w2 (second display).
  // Should only print hierarchy for window under mouse (which is Window2).
  // Uses right button to verify Env::IsMouseButtonDown() works with other
  // buttons.
  {
    auto* generator = GetEventGenerator();
    generator->MoveMouseToCenterOf(w2.get());
    generator->PressRightButton();
    ASSERT_TRUE(aura::Env::GetInstance()->IsMouseButtonDown());

    std::ostringstream out;
    std::vector<std::string> titles = debug::PrintWindowHierarchy(&out, false);
    EXPECT_FALSE(std::ranges::contains(titles, "Window1"));
    EXPECT_TRUE(std::ranges::contains(titles, "Window2"));
    generator->ReleaseRightButton();
  }

  // Case 5: Mouse button IS pressed, mouse is over Window1, but Window1 is
  // HTCAPTION. Should fallback to default behavior (print both Window1 and
  // Window2) because HTCAPTION is excluded.
  {
    auto* delegate =
        static_cast<aura::test::TestWindowDelegate*>(w1->delegate());
    delegate->set_window_component(HTCAPTION);

    auto* generator = GetEventGenerator();
    generator->MoveMouseToCenterOf(w1.get());
    generator->PressLeftButton();
    ASSERT_TRUE(aura::Env::GetInstance()->IsMouseButtonDown());

    std::ostringstream out;
    std::vector<std::string> titles = debug::PrintWindowHierarchy(&out, false);
    EXPECT_TRUE(std::ranges::contains(titles, "Window1"));
    EXPECT_TRUE(std::ranges::contains(titles, "Window2"));

    // Reset delegate component to HTCLIENT.
    delegate->set_window_component(HTCLIENT);
    generator->ReleaseLeftButton();
  }
}

}  // namespace ash
