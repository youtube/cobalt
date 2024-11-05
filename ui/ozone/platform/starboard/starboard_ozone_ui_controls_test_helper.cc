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

#include "ui/ozone/platform/starboard/starboard_ozone_ui_controls_test_helper.h"

#include "ui/aura/window_tree_host.h"

namespace ui {

StarboardOzoneUIControlsTestHelper:: ~StarboardOzoneUIControlsTestHelper() {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::~StarboardOzoneUIControlsTestHelper";
}

void StarboardOzoneUIControlsTestHelper::Reset() {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::Reset";
}

bool StarboardOzoneUIControlsTestHelper::SupportsScreenCoordinates() const {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::SupportsScreenCoordinates";
  return true;
}

unsigned StarboardOzoneUIControlsTestHelper::ButtonDownMask() const {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::ButtonDownMask";

  // dummy
  return 123;
}

void StarboardOzoneUIControlsTestHelper::SendKeyEvents(gfx::AcceleratedWidget widget,
                                                 ui::KeyboardCode key,
                                                 int key_event_types,
                                                 int accelerator_state,
                                                 base::OnceClosure closure) {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::SendKeyEvents";
}

void StarboardOzoneUIControlsTestHelper::SendMouseMotionNotifyEvent(
    gfx::AcceleratedWidget widget,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_loc_in_screen,
    base::OnceClosure closure) {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::SendMouseMotionNotifyEvent";
}

void StarboardOzoneUIControlsTestHelper::SendMouseEvent(
    gfx::AcceleratedWidget widget,
    ui_controls::MouseButton type,
    int button_state,
    int accelerator_state,
    const gfx::Point& mouse_loc,
    const gfx::Point& mouse_loc_in_screen,
    base::OnceClosure closure) {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::SendMouseEvent";

}

void StarboardOzoneUIControlsTestHelper::RunClosureAfterAllPendingUIEvents(
    base::OnceClosure closure) {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::RunClosureAfterAllPendingUIEvents";
}


bool StarboardOzoneUIControlsTestHelper::MustUseUiControlsForMoveCursorTo() {
  LOG(INFO) << "StarboardOzoneUIControlsTestHelper::MustUseUiControlsForMoveCursorTo";
  return false;
}

OzoneUIControlsTestHelper* CreateOzoneUIControlsTestHelperStarboard() {
  LOG(INFO) << "CreateOzoneUIControlsTestHelperStarboard";
  return new StarboardOzoneUIControlsTestHelper();
}


}  // namespace ui
