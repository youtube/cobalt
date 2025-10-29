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

#ifndef INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_INTERNAL_H_
#define INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_INTERNAL_H_

#include "starboard/common/spin_lock.h"
#include "starboard/extension/ui_navigation.h"

namespace starboard {
namespace shared {
namespace uikit {

struct UiNavItemContainer;

}  // namespace uikit
}  // namespace shared
}  // namespace starboard

// This is the base class representing a UI navigation item. Instances should
// only be created via one of the subclasses.
struct SbUiNavItemPrivate {
 public:
  // These methods apply to both focus items and containers.
  virtual void Destroy() = 0;
  virtual void SetDir(SbUiNavItemDir dir) = 0;
  virtual void SetFocusDuration(float seconds) = 0;
  virtual void SetSize(float width, float height) = 0;
  virtual void SetTransform(const SbUiNavMatrix2x3* transform) = 0;
  virtual bool GetFocusTransform(SbUiNavMatrix4* out_transform) = 0;
  virtual bool GetFocusVector(float* out_x, float* out_y) = 0;
  virtual void SetContainer(
      starboard::shared::uikit::UiNavItemContainer* container) = 0;
  virtual void SetContentOffset(float x, float y) = 0;
  virtual void GetContentOffset(float* out_x, float* out_y) = 0;

  // These methods invoke callbacks for various events on the navigation item.
  void OnBlur() { callbacks->onblur(this, callback_context); }
  void OnFocus() { callbacks->onfocus(this, callback_context); }
  void OnScroll() { callbacks->onscroll(this, callback_context); }

  const SbUiNavItemType type;

  starboard::SpinLock lock;

  // This controls whether callbacks are issued.
  bool callbacks_enabled = false;

  // These data members should only be accessed from the main thread.

  // Only enabled navigation items are interactable.
  bool enabled = false;

  // This UIKit object is used to interface with the focus engine.
  void* uikit_object = nullptr;

  // If this item is a content of another navigation item, then |container|
  // points to that other item; otherwise, it is nullptr.
  starboard::shared::uikit::UiNavItemContainer* container = nullptr;

 protected:
  SbUiNavItemPrivate(SbUiNavItemType item_type,
                     const SbUiNavCallbacks* callbacks,
                     void* callback_context)
      : type(item_type),
        callbacks(callbacks),
        callback_context(callback_context) {}
  virtual ~SbUiNavItemPrivate() {}

 private:
  const SbUiNavCallbacks* const callbacks;
  void* const callback_context;
};

#endif  // INTERNAL_STARBOARD_SHARED_UIKIT_UI_NAV_INTERNAL_H_
