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

#include "starboard/shared/uikit/ui_nav_item_focus.h"

#import <UIKit/UIKit.h>

#include "starboard/shared/uikit/ui_nav_item_container.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"

#import "starboard/shared/uikit/defines.h"

using starboard::ScopedSpinLock;
using starboard::shared::uikit::UiNavItemFocus;

namespace {
const SbUiNavMatrix4 kIdentityTransform4 = {{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                                             0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                                             0.0f, 0.0f, 0.0f, 1.0f}};
}  // namespace

// This is a workaround to ensure that the focused item is kept alive until
// unfocused. This should only be accessed from the tvOS main thread. Keep
// track of multiple focus items in case |didUpdateFocusInContext| is called
// on the |nextFocusedItem| before the |previouslyFocusedItem|.
UIView* g_focus_previous;
UIView* g_focus_current;

// This UIKit object shadows the UiNavItemFocus in the UIKit focus engine's
// world. The corresponding navigation item must outlive this UIKit object.
// NOTE: All operations on UIKit objects should occur on the main thread.
@interface SBDUiNavItemFocus : UIView
- (instancetype)initWithNavItem:(UiNavItemFocus*)navItem;
@end

@implementation SBDUiNavItemFocus {
  UiNavItemFocus* _navItem;
  int64_t _focusTimestamp;
}

- (instancetype)initWithNavItem:(UiNavItemFocus*)navItem {
  SB_DCHECK(navItem);
  self = [super init];
  if (self) {
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitNone;
    _navItem = navItem;
  }
  return self;
}

- (void)dealloc {
  delete _navItem;
}

- (void)didHintFocusMovement:(UIFocusMovementHint*)hint {
  CGFloat scale = UIScreen.mainScreen.scale;  // Points-to-pixels scale.
  ScopedSpinLock scoped_lock(_navItem->lock);

  // Translation must be scaled from points to pixels.
  CATransform3D transform = hint.interactionTransform;
  _navItem->focus_transform.m[0] = transform.m11;
  _navItem->focus_transform.m[1] = transform.m21;
  _navItem->focus_transform.m[2] = transform.m31;
  _navItem->focus_transform.m[3] = transform.m41 * scale;
  _navItem->focus_transform.m[4] = transform.m12;
  _navItem->focus_transform.m[5] = transform.m22;
  _navItem->focus_transform.m[6] = transform.m32;
  _navItem->focus_transform.m[7] = transform.m42 * scale;
  _navItem->focus_transform.m[8] = transform.m13;
  _navItem->focus_transform.m[9] = transform.m23;
  _navItem->focus_transform.m[10] = transform.m33;
  _navItem->focus_transform.m[11] = transform.m43 * scale;
  _navItem->focus_transform.m[12] = transform.m14;
  _navItem->focus_transform.m[13] = transform.m24;
  _navItem->focus_transform.m[14] = transform.m34;
  _navItem->focus_transform.m[15] = transform.m44;

  _navItem->focus_vector_x = hint.movementDirection.dx;
  _navItem->focus_vector_y = hint.movementDirection.dy;
}

- (BOOL)shouldUpdateFocusInContext:(UIFocusUpdateContext*)context {
  if (context.previouslyFocusedItem == self) {
    ScopedSpinLock scoped_lock(_navItem->lock);
    if (starboard::CurrentMonotonicTime() < _focusTimestamp + _navItem->focus_duration) {
      return NO;
    }
  }
  return YES;
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext*)context
       withAnimationCoordinator:(UIFocusAnimationCoordinator*)coordinator {
  if (context.previouslyFocusedItem == self) {
    {
      ScopedSpinLock scoped_lock(_navItem->lock);
      _navItem->is_focused = false;
      if (_navItem->callbacks_enabled) {
        _navItem->OnBlur();
      }
    }
    if (g_focus_previous == self) {
      g_focus_previous = nil;
    }
    if (g_focus_current == self) {
      g_focus_current = nil;
    }
  } else if (context.nextFocusedItem == self) {
    g_focus_previous = g_focus_current;
    g_focus_current = self;
    _focusTimestamp = starboard::CurrentMonotonicTime();
    ScopedSpinLock scoped_lock(_navItem->lock);
    _navItem->is_focused = true;
    if (_navItem->callbacks_enabled) {
      _navItem->OnFocus();
    }
  }
}

- (BOOL)canBecomeFocused {
  return _navItem->enabled;
}

- (BOOL)isUserInteractionEnabled {
  return _navItem->enabled;
}
@end

namespace starboard {
namespace shared {
namespace uikit {

UiNavItemFocus::UiNavItemFocus(const SbUiNavCallbacks* callbacks,
                               void* callback_context)
    : SbUiNavItemPrivate(kSbUiNavItemTypeFocus, callbacks, callback_context),
      focus_transform(kIdentityTransform4) {
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      SBDUiNavItemFocus* thisItem =
          [[SBDUiNavItemFocus alloc] initWithNavItem:this];
      // Retain a reference to the Objective-C object.
      uikit_object = (__bridge_retained void*)thisItem;
    });
  }
}

void UiNavItemFocus::Destroy() {
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      // Automatically unregister this content item.
      SetContainer(nullptr);

      // Release the reference to the Objective-C object.
      SBDUiNavItemFocus* thisItem =
          (__bridge_transfer SBDUiNavItemFocus*)uikit_object;
      thisItem = nil;
    });
  }
}

void UiNavItemFocus::SetDir(SbUiNavItemDir dir) {}

void UiNavItemFocus::SetFocusDuration(float seconds) {
  ScopedSpinLock scoped_lock(lock);
  focus_duration = static_cast<int64_t>(seconds * 1000000);
}

void UiNavItemFocus::SetSize(float width, float height) {
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      CGFloat scale = UIScreen.mainScreen.scale;  // Points-to-pixels scale.
      SBDUiNavItemFocus* thisItem = (__bridge SBDUiNavItemFocus*)uikit_object;
      thisItem.bounds = CGRectMake(0.0, 0.0, width / scale, height / scale);
      if (enabled && container != nullptr) {
        container->UpdateContentCache();
      }
    });
  }
}

void UiNavItemFocus::SetTransform(const SbUiNavMatrix2x3* transform) {
  // Make a copy of the transform since it'll be used in another thread.
  SbUiNavMatrix2x3 transform_copy = *transform;

  {
    ScopedSpinLock scoped_lock(lock);
    focus_transform = kIdentityTransform4;
  }

  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      CGFloat scale = UIScreen.mainScreen.scale;  // Points-to-pixels scale.
      SBDUiNavItemFocus* thisItem = (__bridge SBDUiNavItemFocus*)uikit_object;
      thisItem.transform = CGAffineTransformMake(
          transform_copy.m[0], transform_copy.m[3], transform_copy.m[1],
          transform_copy.m[4], transform_copy.m[2] / scale,
          transform_copy.m[5] / scale);
      if (enabled && container != nullptr) {
        container->UpdateContentCache();
      }
    });
  }
}

bool UiNavItemFocus::GetFocusTransform(SbUiNavMatrix4* out_transform) {
  ScopedSpinLock scoped_lock(lock);
  if (!is_focused) {
    return false;
  }
  *out_transform = focus_transform;
  return true;
}

bool UiNavItemFocus::GetFocusVector(float* out_x, float* out_y) {
  ScopedSpinLock scoped_lock(lock);
  if (!is_focused) {
    return false;
  }
  *out_x = focus_vector_x;
  *out_y = focus_vector_y;
  return true;
}

void UiNavItemFocus::SetContainer(UiNavItemContainer* new_container) {
  SB_DCHECK([NSThread isMainThread]);
  if (container == new_container) {
    return;
  }

  if (container != nullptr) {
    SB_DCHECK(new_container == nullptr);
    auto old_container = container;
    container->UnregisterContent(this);
    container = nullptr;
    if (enabled) {
      old_container->UpdateContentCache();
    }
  } else {
    SB_DCHECK(new_container != nullptr);
    new_container->RegisterContent(this);
    container = new_container;
    if (enabled) {
      new_container->UpdateContentCache();
    }
  }
}

void UiNavItemFocus::SetContentOffset(float x, float y) {}

void UiNavItemFocus::GetContentOffset(float* out_x, float* out_y) {
  *out_x = 0.0f;
  *out_y = 0.0f;
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
