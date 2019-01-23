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

// Module Overview: User Interface Navigation module
//
// API to allow applications to take advantage of the platform's native UI
// engine. This is mainly to drive the animation of visual elements and to
// signal which of those elements have focus. The implementation should not
// render any visual elements; instead, it will be used to guide the app in
// where these elements should be drawn.
//
// The app will start by specifying callbacks via SbUiNavSetCallbacks.
// These callbacks may be invoked from any thread.
//
// When the application creates the user interface, it will create navigation
// items via SbUiNavCreateItem for interactable elements. Additionally, the app
// will specify the position, size, content size (if applicable), etc. of these
// navigation items. As the application changes the user interface, it will
// create and destroy navigation items as appropriate. It is possible for items
// to persist between changes in the user interface.
//
// NOTE: A SbUiNavItem should not become interactive until it has been
// enabled via SbUiNavEnableItem. Likewise, an item should become non-
// interactive as soon as it is disabled via SbUiNavDisableItem.
//
// For each render frame, the app will query the local transform for each
// SbUiNavItem in case the native UI engine moves individual items in response
// to user interaction. If the navigation item is a container, then the content
// offset will also be queried to determine the placement of its content items.

#ifndef STARBOARD_UI_NAVIGATION_H_
#define STARBOARD_UI_NAVIGATION_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"
#include "starboard/window.h"

#if SB_API_VERSION >= SB_UI_NAVIGATION_VERSION

#ifdef __cplusplus
extern "C" {
#endif

// --- Types -----------------------------------------------------------------

// An opaque handle to an implementation-private structure representing a
// navigation item.
typedef struct SbUiNavItemPrivate* SbUiNavItem;

// Navigation items may be one of the following types. This must be specified
// upon item creation and may not change during the item's lifespan.
typedef enum SbUiNavItemType {
  // This is a single focusable item.
  kSbUiNavItemTypeFocus,

  // This is a container of navigation items which can also be containers
  // themselves or focusable items.
  kSbUiNavItemTypeContainer,
} SbUiNavItemType;

// This represents a 4x4 transform matrix in row-major order.
typedef struct SbUiNavTransform {
  float m[16];
} SbUiNavTransform;

// This structure specifies all the callbacks which the platform UI engine
// should invoke for various interaction events on navigation items. These
// callbacks may be invoked from any thread at any frequency. The
// |callback_context| is the value that was passed to set_callbacks(), and the
// |item_context| is the value that was passed to create_item() or
// create_root_item().
typedef struct SbUiNavCallbacks {
  // Invoke when an item has lost focus.
  void (*onblur)(SbUiNavItem item, void* callback_context, void* item_context);

  // Invoke when an item has gained focus.
  void (*onfocus)(SbUiNavItem item, void* callback_context, void* item_context);

  // Invoke when an item's content offset is changed. This should only occur
  // for navigation items which contain other items.
  void (*onscroll)(SbUiNavItem item, void* callback_context, void* item_context,
                   float content_offset_x, float content_offset_y);
} SbUiNavCallbacks;

// This structure declares the interface to the UI navigation implementation.
// All function pointers must be specified if the platform supports UI
// navigation.
typedef struct SbUiNavInterface {
  // Used to specify callbacks to be invoked for various interaction events on
  // particular navigation items.
  void (*set_callbacks)(SbUiNavCallbacks callbacks, void* callback_context);

  // Create a new navigation item. When the user interacts with this item
  // the appropriate SbUiNavCallbacks callback will be invoked with the
  // provided |item_context|. An item is not interactable until it is enabled.
  SbUiNavItem (*create_item)(SbUiNavItemType type, void* item_context);

  // This creates a root navigation item container associated with the given
  // |window|. Only navigation items which are transitively contents of this
  // root item are interactable. Only one root item will ever be used at any
  // given time.
  SbUiNavItem (*create_root_item)(SbWindow window, void* item_context);

  // Destroy the given navigation item. If this is a content of another item,
  // then it will first be unregistered. Additionally, if this item contains
  // other items, then those will be unregistered as well, but they will not
  // be automatically destroyed.
  void (*destroy_item)(SbUiNavItem item);

  // This is used to manually force focus on a particular navigation item. Any
  // previously focused navigation item should receive the blur event.
  void (*set_focus)(SbUiNavItem item);

  // This is used to enable or disable user interaction with the specified
  // navigation item. All navigation items are disabled when created, and
  // they must be explicitly enabled to allow user interaction. If a container
  // is disabled, then all of its contents are not interactable even though
  // they remain enabled. If |enabled| is false, it must be guaranteed that
  // once this function returns, no callbacks associated with this item will
  // be invoked until the item is re-enabled.
  void (*set_item_enabled)(SbUiNavItem item, bool enabled);

  // Set the interactable size of the specified navigation item. By default,
  // an item's size is (0,0).
  void (*set_item_size)(SbUiNavItem item, float width, float height);

  // Set the position of the top-left corner for the specified navigation item.
  // Distance is measured in pixels with the origin being the top-left of the
  // screen or top-left of the containing item. By default, an item's position
  // is (0,0).
  void (*set_item_position)(SbUiNavItem item, float x, float y);

  // Retrieve the local transform matrix for the navigation item. The UI
  // engine may translate, rotate, and/or tilt focus items to reflect user
  // interaction. Apply this transform to the focus item's vertices (expressed
  // as offsets from the item center). Return false if the item position
  // should not be changed (i.e. the transform should be treated as identity).
  bool (*get_item_local_transform)(SbUiNavItem item,
      SbUiNavTransform* out_transform);

  // A container navigation item may contain other navigation items. However,
  // it is an error to have circular containment or for |container_item| to not
  // be of type kSbUiNavItemTypeContainer. If |content_item| is already
  // registered with a different container item, then do nothing and return
  // false.
  //
  // The content items' positions are specified relative to the container
  // item's position, and those positions can be further modified by the
  // container's "content offset".
  //
  // For example, consider item A with position (5,5) and content offset (0,0).
  // Given item B with position (10,10) is registered as a content of item A.
  // 1. Item B should be drawn at position (15,15) even though
  //    get_item_position() for B still reports (10,10).
  // 2. If item A's content offset is changed to (10,0), then item B should be
  //    drawn at position (5,15). get_item_position() for B should still report
  //    (10,10).
  //
  // Essentially, content items should be drawn at:
  //   [container position] + [content position] - [container content offset]
  bool (*register_item_content)(SbUiNavItem container_item,
      SbUiNavItem content_item);

  // Unregister the given |content_item| from its container if any. If a
  // navigation item is not explicitly unregistered from its container, it
  // should be automatically unregistered when the item is destroyed.
  void (*unregister_item_as_content)(SbUiNavItem content_item);

  // Set the current content offset for the given container. This may be used
  // to force scrolling to make certain content items visible. A container
  // item's content offset helps determine where its content items should be
  // drawn. Essentially, a content item should be drawn at:
  //   [container position] + [content position] - [container content offset]
  // By default, the content offset is (0,0).
  void (*set_item_content_offset)(SbUiNavItem item,
      float content_offset_x, float content_offset_y);

  // Retrieve the current content offset for the navigation item.
  void (*get_item_content_offset)(SbUiNavItem item,
      float* out_content_offset_x, float* out_content_offset_y);
} SbUiNavInterface;

// --- Constants -------------------------------------------------------------

// Well-defined value for an invalid navigation item.
#define kSbUiNavItemInvalid ((SbUiNavItem)NULL)

// --- Functions -------------------------------------------------------------

// Returns whether the given navigation item handle is valid.
static SB_C_INLINE bool SbUiNavItemIsValid(SbUiNavItem item) {
  return item != kSbUiNavItemInvalid;
}

// Retrieve the platform's UI navigation implementation. If the platform does
// not provide one, then return false without modifying |interface|. Otherwise,
// return true and all members of |interface| must be initialized.
SB_EXPORT bool SbUiNavGetInterface(SbUiNavInterface* interface);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_API_VERSION >= SB_UI_NAVIGATION_VERSION

#endif  // STARBOARD_UI_NAVIGATION_H_
