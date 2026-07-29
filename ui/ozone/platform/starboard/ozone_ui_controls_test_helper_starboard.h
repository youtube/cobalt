// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef UI_OZONE_PLATFORM_STARBOARD_OZONE_UI_CONTROLS_TEST_HELPER_STARBOARD_H_
#define UI_OZONE_PLATFORM_STARBOARD_OZONE_UI_CONTROLS_TEST_HELPER_STARBOARD_H_

#include "ui/ozone/public/ozone_ui_controls_test_helper.h"

namespace ui {

class OzoneUIControlsTestHelperStarboard : public OzoneUIControlsTestHelper {
 public:
  OzoneUIControlsTestHelperStarboard();

  OzoneUIControlsTestHelperStarboard(
      const OzoneUIControlsTestHelperStarboard&) = delete;
  OzoneUIControlsTestHelperStarboard& operator=(
      const OzoneUIControlsTestHelperStarboard&) = delete;

  ~OzoneUIControlsTestHelperStarboard() override;

  void Reset() override;

  bool SupportsScreenCoordinates() const override;

  unsigned ButtonDownMask() const override;

  void SendKeyEvents(gfx::AcceleratedWidget widget,
                     ui::KeyboardCode key,
                     int key_event_types,
                     int accelerator_state,
                     base::OnceClosure closure) override;

  void SendMouseMotionNotifyEvent(gfx::AcceleratedWidget widget,
                                  const gfx::Point& mouse_loc,
                                  const gfx::Point& mouse_loc_in_screen,
                                  base::OnceClosure closure) override;

  void SendMouseEvent(gfx::AcceleratedWidget widget,
                      ui_controls::MouseButton type,
                      int button_state,
                      int accelerator_state,
                      const gfx::Point& mouse_loc,
                      const gfx::Point& mouse_loc_in_screen,
                      base::OnceClosure closure) override;

  void RunClosureAfterAllPendingUIEvents(base::OnceClosure closure) override;

  bool MustUseUiControlsForMoveCursorTo() override;
};

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperStarboard();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_OZONE_UI_CONTROLS_TEST_HELPER_STARBOARD_H_
