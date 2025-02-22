// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_ITEM_FOCUS_H_
#define INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_ITEM_FOCUS_H_

#include "starboard/shared/uikit/ui_nav_internal.h"

namespace starboard {
namespace shared {
namespace uikit {

// This represents a focusable item.
struct UiNavItemFocus : public SbUiNavItemPrivate {
  UiNavItemFocus(const SbUiNavCallbacks* callbacks, void* callback_context);

  // These are SbUiNavItemPrivate methods.
  void Destroy() override;
  void SetDir(SbUiNavItemDir dir) override;
  void SetFocusDuration(float seconds) override;
  void SetSize(float width, float height) override;
  void SetTransform(const SbUiNavMatrix2x3* transform) override;
  bool GetFocusTransform(SbUiNavMatrix4* out_transform) override;
  bool GetFocusVector(float* out_x, float* out_y) override;
  void SetContainer(UiNavItemContainer* container) override;
  void SetContentOffset(float x, float y) override;
  void GetContentOffset(float* out_x, float* out_y) override;

  // When this item is focused, it should remain focused until the focus
  // duration (microseconds) has elapsed or focus is manually updated.
  int64_t focus_duration = 0;

  // The focus engine may shift the location of focus items during interaction.
  // This is used to influence the navigation item's position.
  SbUiNavMatrix4 focus_transform;

  // The focus vector provides information about the direction in which focus
  // is about to move. This provides feedback for user input that is too small
  // to change focus.
  float focus_vector_x = 0.0f;
  float focus_vector_y = 0.0f;

  // This records whether the navigation item currently has focus.
  bool is_focused = false;
};

}  // namespace uikit
}  // namespace shared
}  // namespace starboard

#endif  // INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_ITEM_FOCUS_H_
