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

NativeItem CreateItem(NativeItemType type,
                      const NativeCallbacks* callbacks,
                      void* callback_context) {
  SB_UNREFERENCED_PARAMETER(callbacks);
  SB_UNREFERENCED_PARAMETER(callback_context);
  return reinterpret_cast<NativeItem>(new ItemImpl(type));
}

void DestroyItem(NativeItem item) {
  delete reinterpret_cast<ItemImpl*>(item);
}

void SetFocus(NativeItem item) {
  SB_UNREFERENCED_PARAMETER(item);
}

void SetItemEnabled(NativeItem item, bool enabled) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(enabled);
}

void SetItemSize(NativeItem item, float width, float height) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(width);
  SB_UNREFERENCED_PARAMETER(height);
}

void SetItemTransform(NativeItem item, const NativeMatrix2x3* transform) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(transform);
}

bool GetItemFocusTransform(NativeItem item, NativeMatrix4* out_transform) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(out_transform);
  return false;
}

bool GetItemFocusVector(NativeItem item, float* out_x, float* out_y) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(out_x);
  SB_UNREFERENCED_PARAMETER(out_y);
  return false;
}

void SetItemContainerWindow(NativeItem item, SbWindow window) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(window);
}

void SetItemContainerItem(NativeItem item, NativeItem container) {
  SB_UNREFERENCED_PARAMETER(item);
  SB_UNREFERENCED_PARAMETER(container);
}

void SetItemContentOffset(NativeItem item,
    float content_offset_x, float content_offset_y) {
  ItemImpl* stub_item = reinterpret_cast<ItemImpl*>(item);
  if (stub_item->type == kNativeItemTypeContainer) {
    starboard::ScopedSpinLock scoped_lock(stub_item->lock);
    stub_item->content_offset_x = content_offset_x;
    stub_item->content_offset_y = content_offset_y;
  }
}

void GetItemContentOffset(NativeItem item,
    float* out_content_offset_x, float* out_content_offset_y) {
  ItemImpl* stub_item = reinterpret_cast<ItemImpl*>(item);
  starboard::ScopedSpinLock scoped_lock(stub_item->lock);
  *out_content_offset_x = stub_item->content_offset_x;
  *out_content_offset_y = stub_item->content_offset_y;
}

NativeInterface InitializeInterface() {
  NativeInterface interface = { 0 };
#if SB_API_VERSION >= SB_UI_NAVIGATION_VERSION
  if (SbUiNavGetInterface(&interface)) {
    return interface;
  }
#endif

  interface.create_item = &CreateItem;
  interface.destroy_item = &DestroyItem;
  interface.set_focus = &SetFocus;
  interface.set_item_enabled = &SetItemEnabled;
  interface.set_item_size = &SetItemSize;
  interface.set_item_transform = &SetItemTransform;
  interface.get_item_focus_transform = &GetItemFocusTransform;
  interface.get_item_focus_vector = &GetItemFocusVector;
  interface.set_item_container_window = &SetItemContainerWindow;
  interface.set_item_container_item = &SetItemContainerItem;
  interface.set_item_content_offset = &SetItemContentOffset;
  interface.get_item_content_offset = &GetItemContentOffset;
  return interface;
}

}  // namespace

const NativeInterface& GetInterface() {
  static NativeInterface s_interface = InitializeInterface();
  return s_interface;
}

}  // namespace ui_navigation
}  // namespace cobalt
