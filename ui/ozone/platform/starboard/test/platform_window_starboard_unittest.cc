// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ui/ozone/platform/starboard/platform_window_starboard.h"

#include "base/functional/bind.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/starboard/platform_screen_starboard.h"
#include "ui/ozone/platform/starboard/test/starboard_test_helper.h"

namespace ui {
namespace {

// Helper function to create a basic event.
ui::Event* CreateTestEvent() {
  return new ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(0, 0),
                            gfx::Point(0, 0), base::TimeTicks::Now(), 0, 0);
}

TEST(PlatformWindowStarboardTest, CreateAndDestroy) {
  PlatformScreenStarboard screen;
  screen.InitScreen();

  TestPlatformWindowDelegate delegate;
  SbWindow created_window = kSbWindowInvalid;
  ui::PlatformWindowStarboard::SetWindowCreatedCallback(base::BindRepeating(
      [](SbWindow* out_window, SbWindow window) { *out_window = window; },
      &created_window));

  constexpr gfx::Rect kBounds(100, 200, 300, 400);
  std::unique_ptr<ui::PlatformWindowStarboard> window =
      std::make_unique<ui::PlatformWindowStarboard>(&delegate, kBounds);

  // Check initial bounds and screen bounds against the native Starboard window
  // size when no command-line override switch is present.
  EXPECT_TRUE(SbWindowIsValid(created_window));
  SbWindowSize sb_size{};
  EXPECT_TRUE(SbWindowGetSize(created_window, &sb_size));
  const gfx::Rect expected_bounds(kBounds.x(), kBounds.y(), sb_size.width,
                                  sb_size.height);
  EXPECT_EQ(window->GetBoundsInPixels(), expected_bounds);
  EXPECT_EQ(screen.GetPrimaryDisplay().bounds(), expected_bounds);

  // Verify Minimize() and Restore() recreate the native window and notify the
  // delegate if the bounds changed prior to Restore().
  window->SetBoundsInPixels(kBounds);
  EXPECT_EQ(delegate.bounds_changed_count(), 1);
  window->Minimize();
  EXPECT_FALSE(window->IsVisible());
  window->Restore();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(window->GetBoundsInPixels(), expected_bounds);
  EXPECT_EQ(screen.GetPrimaryDisplay().bounds(), expected_bounds);
  if (expected_bounds != kBounds) {
    EXPECT_EQ(delegate.bounds_changed_count(), 2);
  }

  ui::PlatformWindowStarboard::ClearWindowCreatedCallback();

  // Destroy the window.
  window->Close();
  EXPECT_TRUE(delegate.closed());
}

TEST(PlatformWindowStarboardTest, SetBoundsInPixels) {
  TestPlatformWindowDelegate delegate;
  constexpr gfx::Rect kInitialBounds(100, 200, 300, 400);
  std::unique_ptr<ui::PlatformWindowStarboard> window =
      std::make_unique<ui::PlatformWindowStarboard>(&delegate, kInitialBounds);

  constexpr gfx::Rect kNewBounds(150, 250, 350, 450);
  window->SetBoundsInPixels(kNewBounds);

  EXPECT_EQ(window->GetBoundsInPixels(), kNewBounds);
  EXPECT_EQ(delegate.bounds_changed_count(), 1);

  // Set the same bounds.  The delegate should still be notified.
  window->SetBoundsInPixels(kNewBounds);
  EXPECT_EQ(delegate.bounds_changed_count(), 2);
}

TEST(PlatformWindowStarboardTest, DispatchEvent) {
  TestPlatformWindowDelegate delegate;
  constexpr gfx::Rect kBounds(100, 200, 300, 400);
  std::unique_ptr<ui::PlatformWindowStarboard> window =
      std::make_unique<ui::PlatformWindowStarboard>(&delegate, kBounds);

  ui::Event* event = CreateTestEvent();
  const auto result = window->DispatchEvent(event);
  const auto dispatched_event_type = delegate.dispatched_event_type();
  EXPECT_TRUE(dispatched_event_type.has_value());
  EXPECT_EQ(dispatched_event_type.value(), ui::EventType::kMousePressed);

  EXPECT_EQ(result, ui::POST_DISPATCH_STOP_PROPAGATION);

  delete event;
}

TEST(PlatformWindowStarboardTest, ProcessWindowSizeChangedEvent) {
  TestPlatformWindowDelegate delegate;
  constexpr gfx::Rect kInitial_bounds(100, 200, 300, 400);
  std::unique_ptr<ui::PlatformWindowStarboard> window =
      std::make_unique<ui::PlatformWindowStarboard>(&delegate, kInitial_bounds);

  constexpr int kNewWidth = 500;
  constexpr int kNewHeight = 600;
  window->ProcessWindowSizeChangedEvent(kNewWidth, kNewHeight);

  constexpr gfx::Rect kExpected_bounds(100, 200, 500, 600);
  EXPECT_EQ(window->GetBoundsInPixels(), kExpected_bounds);
  EXPECT_EQ(delegate.bounds_changed_count(), 1);
}

TEST(PlatformWindowStarboardTest,
     CreateWithHostWindowSizeSwitchUsesRequestedBoundsAndUpdatesScreen) {
  // Append the host window size switch via ScopedCommandLine to exercise the
  // explicit override path in PlatformWindowStarboard::Show().
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "content-shell-host-window-size", "300x400");

  // Initialize a PlatformScreenStarboard instance to verify that creating and
  // resizing the window updates the primary display bounds.
  PlatformScreenStarboard screen;
  screen.InitScreen();

  SbWindow created_window = kSbWindowInvalid;
  ui::PlatformWindowStarboard::SetWindowCreatedCallback(base::BindRepeating(
      [](SbWindow* out_window, SbWindow window) { *out_window = window; },
      &created_window));

  TestPlatformWindowDelegate delegate;
  constexpr gfx::Rect kBounds(0, 0, 300, 400);
  std::unique_ptr<ui::PlatformWindowStarboard> window =
      std::make_unique<ui::PlatformWindowStarboard>(&delegate, kBounds);

  // Verify that both the window bounds and primary screen display bounds match
  // the size reported by SbWindowGetSize().
  EXPECT_TRUE(SbWindowIsValid(created_window));
  SbWindowSize sb_size{};
  EXPECT_TRUE(SbWindowGetSize(created_window, &sb_size));
  const gfx::Rect expected_initial_bounds(kBounds.x(), kBounds.y(),
                                          sb_size.width, sb_size.height);
  EXPECT_EQ(window->GetBoundsInPixels(), expected_initial_bounds);
  EXPECT_EQ(screen.GetPrimaryDisplay().bounds(), expected_initial_bounds);

  // Verify that subsequent window size change events update both the window
  // bounds and the primary display bounds in PlatformScreenStarboard.
  constexpr gfx::Rect kResizedBounds(0, 0, 1280, 720);
  window->ProcessWindowSizeChangedEvent(kResizedBounds.width(),
                                        kResizedBounds.height());
  EXPECT_EQ(window->GetBoundsInPixels(), kResizedBounds);
  EXPECT_EQ(screen.GetPrimaryDisplay().bounds(), kResizedBounds);

  ui::PlatformWindowStarboard::ClearWindowCreatedCallback();
}
}  // namespace
}  // namespace ui
