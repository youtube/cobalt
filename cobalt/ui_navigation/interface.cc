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

#include "starboard/common/spin_lock.h"

namespace cobalt {
namespace ui_navigation {

namespace {

struct ItemImpl {
  explicit ItemImpl(NativeItemType type) : type(type) {}

  starboard::SpinLock lock;

  const NativeItemType type;
  float content_offset_x = 0.0f;
  float content_offset_y = 0.0f;
};

NativeItem CreateItem(NativeItemType type, const NativeCallbacks* callbacks,
                      void* callback_context) {
  return reinterpret_cast<NativeItem>(new ItemImpl(type));
}

void DestroyItem(NativeItem item) { delete reinterpret_cast<ItemImpl*>(item); }

void SetFocus(NativeItem item) {}

void SetItemEnabled(NativeItem item, bool enabled) {}

void SetItemDir(NativeItem item, NativeItemDir dir) {}

void SetItemFocusDuration(NativeItem item, float seconds) {}

void SetItemSize(NativeItem item, float width, float height) {}

void SetItemTransform(NativeItem item, const NativeMatrix2x3* transform) {}

bool GetItemFocusTransform(NativeItem item, NativeMatrix4* out_transform) {
  return false;
}

bool GetItemFocusVector(NativeItem item, float* out_x, float* out_y) {
  return false;
}

void SetItemContainerWindow(NativeItem item, SbWindow window) {}

void SetItemContainerItem(NativeItem item, NativeItem container) {}

void SetItemContentOffset(NativeItem item, float content_offset_x,
                          float content_offset_y) {
  ItemImpl* stub_item = reinterpret_cast<ItemImpl*>(item);
  if (stub_item->type == kNativeItemTypeContainer) {
    starboard::ScopedSpinLock scoped_lock(stub_item->lock);
    stub_item->content_offset_x = content_offset_x;
    stub_item->content_offset_y = content_offset_y;
  }
}

void GetItemContentOffset(NativeItem item, float* out_content_offset_x,
                          float* out_content_offset_y) {
  ItemImpl* stub_item = reinterpret_cast<ItemImpl*>(item);
  starboard::ScopedSpinLock scoped_lock(stub_item->lock);
  *out_content_offset_x = stub_item->content_offset_x;
  *out_content_offset_y = stub_item->content_offset_y;
}

void DoBatchUpdate(void (*update_function)(void*), void* context) {
  (*update_function)(context);
}

NativeInterface InitializeInterface() {
  NativeInterface interface = {0};

#if SB_API_VERSION >= 12
  SbUiNavInterface sb_ui_interface = {0};
  if (SbUiNavGetInterface(&sb_ui_interface)) {
    interface.create_item = sb_ui_interface.create_item;
    interface.destroy_item = sb_ui_interface.destroy_item;
    interface.set_focus = sb_ui_interface.set_focus;
    interface.set_item_enabled = sb_ui_interface.set_item_enabled;
    interface.set_item_dir = sb_ui_interface.set_item_dir;
    interface.set_item_size = sb_ui_interface.set_item_size;
    interface.set_item_transform = sb_ui_interface.set_item_transform;
    interface.get_item_focus_transform =
        sb_ui_interface.get_item_focus_transform;
    interface.get_item_focus_vector = sb_ui_interface.get_item_focus_vector;
    interface.set_item_container_window =
        sb_ui_interface.set_item_container_window;
    interface.set_item_container_item = sb_ui_interface.set_item_container_item;
    interface.set_item_content_offset = sb_ui_interface.set_item_content_offset;
    interface.get_item_content_offset = sb_ui_interface.get_item_content_offset;
#if SB_API_VERSION >= 13
    interface.set_item_focus_duration = sb_ui_interface.set_item_focus_duration;
    interface.do_batch_update = sb_ui_interface.do_batch_update;
#else
    interface.set_item_focus_duration = &SetItemFocusDuration;
    interface.do_batch_update = &DoBatchUpdate;
#endif
    return interface;
  }
#endif

  interface.create_item = &CreateItem;
  interface.destroy_item = &DestroyItem;
  interface.set_focus = &SetFocus;
  interface.set_item_enabled = &SetItemEnabled;
  interface.set_item_dir = &SetItemDir;
  interface.set_item_focus_duration = &SetItemFocusDuration;
  interface.set_item_size = &SetItemSize;
  interface.set_item_transform = &SetItemTransform;
  interface.get_item_focus_transform = &GetItemFocusTransform;
  interface.get_item_focus_vector = &GetItemFocusVector;
  interface.set_item_container_window = &SetItemContainerWindow;
  interface.set_item_container_item = &SetItemContainerItem;
  interface.set_item_content_offset = &SetItemContentOffset;
  interface.get_item_content_offset = &GetItemContentOffset;
  interface.do_batch_update = &DoBatchUpdate;
  return interface;
}

}  // namespace

const NativeInterface& GetInterface() {
  static NativeInterface s_interface = InitializeInterface();
  return s_interface;
}

}  // namespace ui_navigation
}  // namespace cobalt
