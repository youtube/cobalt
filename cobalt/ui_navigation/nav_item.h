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

#ifndef COBALT_UI_NAVIGATION_NAV_ITEM_H_
#define COBALT_UI_NAVIGATION_NAV_ITEM_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/ui_navigation/interface.h"
#include "starboard/atomic.h"

namespace cobalt {
namespace ui_navigation {

// This wraps a NativeItem to make it ref-counted.
class NavItem : public base::RefCountedThreadSafe<NavItem> {
 public:
  NavItem(NativeItemType type, const base::Closure& onblur_callback,
          const base::Closure& onfocus_callback,
          const base::Closure& onscroll_callback);

  NativeItemType GetType() const { return nav_item_type_; }

  bool IsContainer() const {
    return nav_item_type_ == kNativeItemTypeContainer;
  }

  // Some actions may be queued for later execution by this function. This is
  // to ensure the UI is updated atomically. This function must be called
  // periodically to ensure all UI updates are executed.
  void PerformQueuedUpdates();

  void Focus();
  void UnfocusAll();
  void SetEnabled(bool enabled);
  void SetDir(NativeItemDir dir);
  void SetFocusDuration(float seconds);
  void SetSize(float width, float height);
  void SetTransform(const NativeMatrix2x3* transform);
  bool GetFocusTransform(NativeMatrix4* out_transform);
  bool GetFocusVector(float* out_x, float* out_y);
  void SetContainerWindow(SbWindow window);
  void SetContainerItem(const scoped_refptr<NavItem>& container);
  const scoped_refptr<NavItem>& GetContainerItem() const;
  void SetContentOffset(float x, float y);
  void GetContentOffset(float* out_x, float* out_y);

  // These functions query the currently-focused nav item, if any.
  static bool GetGlobalFocusTransform(NativeMatrix4* out_transform);
  static bool GetGlobalFocusVector(float* out_x, float* out_y);

 private:
  friend class base::RefCountedThreadSafe<NavItem>;
  ~NavItem();

  static void OnBlur(NativeItem item, void* callback_context);
  static void OnFocus(NativeItem item, void* callback_context);
  static void OnScroll(NativeItem item, void* callback_context);

  base::Closure onblur_callback_;
  base::Closure onfocus_callback_;
  base::Closure onscroll_callback_;

  scoped_refptr<NavItem> container_;

  NativeItemType nav_item_type_;
  NativeItem nav_item_;

  enum State {
    kStateNew,
    kStateEnabled,
    kStatePendingDelete,
  };
  State state_;

  static NativeCallbacks s_callbacks_;
  static SbAtomicPtr s_focused_nav_item_;
};

}  // namespace ui_navigation
}  // namespace cobalt

#endif  // COBALT_UI_NAVIGATION_NAV_ITEM_H_
