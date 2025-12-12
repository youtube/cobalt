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

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/starboard/test/starboard_test_helper.h"

namespace ui {
namespace {

// Helper function to create a basic event.
ui::Event* CreateTestEvent() {
  return new ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(0, 0),
                            gfx::Point(0, 0), base::TimeTicks::Now(), 0, 0);
}

TEST(PlatformWindowStarboardTest, CreateAndDestroy) {
  TestPlatformWindowDelegate delegate;
  constexpr gfx::Rect kBounds(100, 200, 300, 400);
  std::unique_ptr<ui::PlatformWindowStarboard> window =
      std::make_unique<ui::PlatformWindowStarboard>(&delegate, kBounds);

  // Check initial bounds.
  EXPECT_EQ(window->GetBoundsInPixels(), kBounds);

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
}  // namespace
}  // namespace ui
