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

#ifndef INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_ITEM_CONTAINER_H_
#define INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_ITEM_CONTAINER_H_

#include "starboard/shared/uikit/ui_nav_internal.h"

#include <vector>

namespace starboard {
namespace shared {
namespace uikit {

// This represents a focus item container.
struct UiNavItemContainer : public SbUiNavItemPrivate {
  UiNavItemContainer(const SbUiNavCallbacks* callbacks, void* callback_context);

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

  // Force focus onto a navigation item. This is only effective when called on
  // the root container item.
  void FocusContent(SbUiNavItemPrivate* content);

  // Register and unregister contents. This should only be called from
  // SetContainer().
  void RegisterContent(SbUiNavItemPrivate* content);
  void UnregisterContent(SbUiNavItemPrivate* content);

  // Update cached data about the contents. This should only be called from
  // the main thread. This must be called after registering / unregistering
  // contents or enabling / disabling them.
  void UpdateContentCache();

  // This controls the display offset for content items.
  float content_offset_x = 0.0f;
  float content_offset_y = 0.0f;

  // In some situations, content offset should not be updated. Use this counter
  // to facilitate the logic. This should only be accessed when |lock| has been
  // acquired.
  int content_offset_lock_count = 0;

  std::vector<SbUiNavItemPrivate*> content_items;
};

}  // namespace uikit
}  // namespace shared
}  // namespace starboard

#endif  // INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_ITEM_CONTAINER_H_
