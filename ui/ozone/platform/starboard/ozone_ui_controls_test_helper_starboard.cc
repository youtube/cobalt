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

#include "ui/ozone/platform/starboard/ozone_ui_controls_test_helper_starboard.h"

#include "base/functional/callback.h"

namespace ui {

OzoneUIControlsTestHelperStarboard::OzoneUIControlsTestHelperStarboard() {}

OzoneUIControlsTestHelperStarboard::~OzoneUIControlsTestHelperStarboard() {}

void OzoneUIControlsTestHelperStarboard::Reset() {}

bool OzoneUIControlsTestHelperStarboard::SupportsScreenCoordinates() const {
  // TODO(b/371272304): Once PlatformWindowStarboard is fully implemented change
  // this to return true.
  return false;
}

unsigned OzoneUIControlsTestHelperStarboard::ButtonDownMask() const {
  // TODO(b/371272304): This is only called after checking
  // !MustUseUiControlsForMoveCursorTo(). Re-examine how we want tests to handle
  // cursor movement and determine if this should be NOTREACHED() instead of
  // return 0.
  return 0;
}

void OzoneUIControlsTestHelperStarboard::SendKeyEvents(
    gfx::AcceleratedWidget widget,
    ui::KeyboardCode key,
    int key_event_types,
    int accelerator_state,
    base::OnceClosure closure) {
  // TODO(b/371272304): Once PlatformWindowStarboard has an event handler added,
  // update this to send events.
  return;
}

void OzoneUIControlsTestHelperStarboard::SendMouseMotionNotifyEvent(
    gfx::AcceleratedWidget widget,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_loc_in_screen,
    base::OnceClosure closure) {
  // TODO(b/371272304): Once PlatformWindowStarboard has an event handler added,
  // update this to send events.
  return;
}

void OzoneUIControlsTestHelperStarboard::SendMouseEvent(
    gfx::AcceleratedWidget widget,
    ui_controls::MouseButton type,
    int button_state,
    int accelerator_state,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_loc_in_screen,
    base::OnceClosure closure) {
  // TODO(b/371272304): Once PlatformWindowStarboard has an event handler added,
  // update this to send events.
  return;
}

void OzoneUIControlsTestHelperStarboard::RunClosureAfterAllPendingUIEvents(
    base::OnceClosure closure) {
  // TODO(b/371272304): Once PlatformWindowStarboard has an event handler added,
  // update this to send events.
  return;
}

bool OzoneUIControlsTestHelperStarboard::MustUseUiControlsForMoveCursorTo() {
  return false;
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperStarboard() {
  return new OzoneUIControlsTestHelperStarboard();
}

}  // namespace ui
