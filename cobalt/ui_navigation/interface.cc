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

#include "cobalt/ui_navigation/interface.h"

#include "starboard/spin_lock.h"

namespace cobalt {
namespace ui_navigation {

namespace {

struct ItemImpl {
  explicit ItemImpl(SbUiNavItemType type) : type(type) {}

  starboard::SpinLock lock;

  const SbUiNavItemType type;
  float content_offset_x = 0.0f;
  float content_offset_y = 0.0f;
};

SbUiNavItem CreateItem(SbUiNavItemType type,
                       const SbUiNavCallbacks* callbacks,
                       void* callback_context) {
  SB_UNREFERENCED_PARAMETER(callbacks);
  SB_UNREFERENCED_PARAMETER(callback_context);
  return reinterpret_cast<SbUiNavItem>(new ItemImpl(type));
}

void DestroyItem(SbUiNavItem item) {
  delete reinterpret_cast<ItemImpl*>(item);
}

void RegisterRootContainerWithWindow(SbUiNavItem container, SbWindow window) {
  SB_UNREFERENCED_PARAMETER(container);
  SB_UNREFERENCED_PARAMETER(window);
}

void SetFocus(SbUiNavItem item) {
  SB_UNREFERENCED_PARAMETER(item);
}

void SetItemEnabled(SbUiNavItem item, bool enabled) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(enabled);
}

void SetItemSize(SbUiNavItem item, float width, float height) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(width);
  SB_UNREFERENCED_PARAMETER(height);
}

void SetItemPosition(SbUiNavItem item, float x, float y) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(x);
  SB_UNREFERENCED_PARAMETER(y);
}

bool GetItemLocalTransform(SbUiNavItem item, SbUiNavTransform* out_transform) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(out_transform);
  return false;
}

bool RegisterItemContent(SbUiNavItem container_item, SbUiNavItem content_item) {
  SB_UNREFERENCED_PARAMETER(container_item);
  SB_UNREFERENCED_PARAMETER(content_item);
  return true;
}

void UnregisterItemAsContent(SbUiNavItem content_item) {
  SB_UNREFERENCED_PARAMETER(content_item);
}

void SetItemContentOffset(SbUiNavItem item,
    float content_offset_x, float content_offset_y) {
  ItemImpl* stub_item = reinterpret_cast<ItemImpl*>(item);
  if (stub_item->type == kSbUiNavItemTypeContainer) {
    starboard::ScopedSpinLock scoped_lock(stub_item->lock);
    stub_item->content_offset_x = content_offset_x;
    stub_item->content_offset_y = content_offset_y;
  }
}

void GetItemContentOffset(SbUiNavItem item,
    float* out_content_offset_x, float* out_content_offset_y) {
  ItemImpl* stub_item = reinterpret_cast<ItemImpl*>(item);
  starboard::ScopedSpinLock scoped_lock(stub_item->lock);
  *out_content_offset_x = stub_item->content_offset_x;
  *out_content_offset_y = stub_item->content_offset_y;
}

SbUiNavInterface InitializeInterface() {
  SbUiNavInterface interface = { 0 };
#if SB_API_VERSION >= SB_UI_NAVIGATION_VERSION
  if (!SbUiNavGetInterface(&interface))
#endif
  {
    interface.create_item = &CreateItem;
    interface.destroy_item = &DestroyItem;
    interface.register_root_container_with_window =
        &RegisterRootContainerWithWindow;
    interface.set_focus = &SetFocus;
    interface.set_item_enabled = &SetItemEnabled;
    interface.set_item_size = &SetItemSize;
    interface.set_item_position = &SetItemPosition;
    interface.get_item_local_transform = &GetItemLocalTransform;
    interface.register_item_content = &RegisterItemContent;
    interface.unregister_item_as_content = &UnregisterItemAsContent;
    interface.set_item_content_offset = &SetItemContentOffset;
    interface.get_item_content_offset = &GetItemContentOffset;
  }
  return interface;
}

}  // namespace

const SbUiNavInterface& GetInterface() {
  static SbUiNavInterface s_interface = InitializeInterface();
  return s_interface;
}

}  // namespace ui_navigation
}  // namespace cobalt
