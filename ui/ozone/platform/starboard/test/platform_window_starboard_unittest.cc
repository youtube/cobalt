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
// Using OzoneStarboardTest to allow SbWindowCreate and SbWindowDestroy.
class PlatformWindowStarboardTest : public testing::Test {
 public:
  PlatformWindowStarboardTest() {
    sb_window_ =
        std::make_unique<PlatformWindowStarboard>(&delegate_, gfx::Rect(0, 0));
  }

  ~PlatformWindowStarboardTest() {
    // Reset |sb_window_| before parent destructor is called so SbWindowDestroy
    // can run.
    sb_window_.reset();
  }

  PlatformWindowStarboard* window() { return sb_window_.get(); }

 protected:
  MockPlatformWindowDelegate delegate_;

 private:
  // Using a pointer to delay initialization of the window until after starting
  // the main Starboard thread in partent constructor.
  std::unique_ptr<PlatformWindowStarboard> sb_window_;
};

TEST_F(PlatformWindowStarboardTest, CanDispatchEvent) {
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  const PlatformEvent& platform_event = &event;

  EXPECT_TRUE(window()->CanDispatchEvent(platform_event));
}

TEST_F(PlatformWindowStarboardTest, DispatchEvent) {
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0);
  const PlatformEvent& platform_event = &event;

  ui::EventType type;
  EXPECT_CALL(delegate_, DispatchEvent(testing::_))
      .Times(1)
      .WillOnce([&type](const PlatformEvent& event) {
        type = ui::EventTypeFromNative(event);
      });

  auto result = window()->DispatchEvent(platform_event);
  EXPECT_EQ(result, ui::POST_DISPATCH_STOP_PROPAGATION);
  EXPECT_EQ(type, ui::ET_MOUSE_PRESSED);
}
}  // namespace
}  // namespace ui
