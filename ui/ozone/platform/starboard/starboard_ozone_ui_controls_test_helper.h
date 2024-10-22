// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_STARBOARD_STARBOARD_OZONE_UI_CONTROLS_TEST_HELPER_H_
#define UI_OZONE_PLATFORM_STARBOARD_STARBOARD_OZONE_UI_CONTROLS_TEST_HELPER_H_

#include "ui/ozone/public/ozone_ui_controls_test_helper.h"


namespace ui {

class StarboardOzoneUIControlsTestHelper : public OzoneUIControlsTestHelper {
 public:
  StarboardOzoneUIControlsTestHelper()  = default;
  StarboardOzoneUIControlsTestHelper(const StarboardOzoneUIControlsTestHelper&) = delete;
  StarboardOzoneUIControlsTestHelper& operator=(const StarboardOzoneUIControlsTestHelper&) =
      delete;
  ~StarboardOzoneUIControlsTestHelper() override;

  // OzoneUIControlsTestHelper:
  void Reset() override;
  bool SupportsScreenCoordinates() const override;
  unsigned ButtonDownMask() const override;
  void SendKeyEvents(gfx::AcceleratedWidget widget,
                     ui::KeyboardCode key,
                     int key_event_types,
                     int accelerated_state,
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

#endif  // UI_OZONE_PLATFORM_STARBOARD_STARBOARD_OZONE_UI_CONTROLS_TEST_HELPER_H_
