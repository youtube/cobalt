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

#include "starboard/shared/uikit/ui_nav_item_container.h"

#import <UIKit/UIKit.h>
#include <algorithm>

#include "starboard/shared/uikit/application_window.h"
#include "starboard/common/log.h"

#import "starboard/shared/uikit/defines.h"

using starboard::ScopedSpinLock;
using starboard::shared::uikit::UiNavItemContainer;

// This UIKit object shadows the UiNavItemContainer in the UIKit focus engine's
// world. The corresponding navigation item must outlive this UIKit object.
// NOTE: All operations on this object must occur on the main thread.
@interface SBDUiNavItemContainer : UIScrollView
- (instancetype)initWithNavItem:(UiNavItemContainer*)navItem;

// Use this method to stop any scrolling animation that might be occurring.
- (void)stopScrollAnimation;

// This property assists in the lazy update of the contentSize. Set it to YES
// when a change may affect the contentSize.
@property(nonatomic, readwrite) BOOL contentsChanged;

// Containers have directionality which influences scrolling behavior.
@property(nonatomic, readwrite) SbUiNavItemDir dir;
@end

@implementation SBDUiNavItemContainer {
  UiNavItemContainer* _navItem;
  BOOL _hasActiveItems;
}

- (instancetype)initWithNavItem:(UiNavItemContainer*)navItem {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
    _navItem = navItem;
    _dir.is_left_to_right = YES;
    _dir.is_top_to_bottom = YES;
    _contentsChanged = YES;
  }
  return self;
}

- (void)dealloc {
  delete _navItem;
}

- (void)stopScrollAnimation {
  // Use setContentOffset:animated to stop any scrolling animations that may be
  // happening. However, since this object implements special logic in its
  // setContentOffset which should not be used, just call the super.
  [super setContentOffset:self.contentOffset animated:NO];
}

- (void)updateContentCache {
  CGFloat min_x = 0;
  CGFloat max_x = 0;
  CGFloat min_y = 0;
  CGFloat max_y = 0;
  BOOL hasActiveItems = NO;

  for (UIView* view in self.subviews) {
    if (view.userInteractionEnabled) {
      CGRect frame = [self convertRect:view.bounds fromView:view];
      min_x = std::min(min_x, frame.origin.x);
      max_x = std::max(max_x, frame.origin.x + frame.size.width);
      min_y = std::min(min_y, frame.origin.y);
      max_y = std::max(max_y, frame.origin.y + frame.size.height);
      hasActiveItems = YES;
    }
  }

  _hasActiveItems = hasActiveItems;
  _contentsChanged = NO;

  // Use contentSize to allow positive content offsets.
  // Use contentInset to allow negative content offsets (for right-to-left /
  // bottom-to-top).
  CGSize contentSize = CGSizeMake(max_x, max_y);
  UIEdgeInsets contentInset = UIEdgeInsetsZero;

  if (!_dir.is_left_to_right) {
    // Allow scrolling to contents in the bounds and anything to the left
    // (negative horizontal position).
    contentInset.left = -min_x;
    // Honor scroll items' translateX() when computing scroll items position
    // relative to the right edge of the scroll container.
    contentInset.right = self.bounds.size.width - max_x;
  }

  if (!_dir.is_top_to_bottom) {
    // Allow scrolling to contents in the bounds and anything above (negative
    // vertical position).
    contentInset.top = -min_y;
    // Honor scroll items' translateY() when computing scroll items position
    // relative to the bottom edge of the scroll container.
    contentInset.bottom = self.bounds.size.height - max_y;
  }

  self.contentSize = contentSize;
  self.contentInset = contentInset;
}

- (BOOL)isUserInteractionEnabled {
  if (_contentsChanged) {
    [self updateContentCache];
  }
  return _navItem && _navItem->enabled && _hasActiveItems;
}

- (void)setContentOffset:(CGPoint)offset {
  if (!_navItem) {
    // Object hasn't been fully initialized yet. Just initialize the super.
    [super setContentOffset:offset];
    return;
  }

  // Lock the nav item immediately to avoid race conditions with
  // UiNavItemContainer::SetContentOffset().
  // NOTE: Be careful not to call any functions that may attempt to lock this
  //       spinlock again -- that will result in a deadlock.
  ScopedSpinLock scoped_lock(_navItem->lock);

  if (_navItem->content_offset_lock_count > 0) {
    return;
  }
  if (!_navItem->container && self.superview) {
    // The UI navigation root container should not scroll.
    return;
  }

  // Only scroll if this is in the focus chain. The user may temporarily
  // disable navigation items but want the container to maintain its current
  // content offset so that navigation items can seamlessly be re-enabled.
  UIView* focusedView = UIScreen.mainScreen.focusedView;
  if (![focusedView isDescendantOfView:self]) {
    return;
  }

  // Don't scroll if unfocusing. Unfocusing can be inferred if the focused item
  // can't become focused anymore.
  if (focusedView != self && !focusedView.canBecomeFocused) {
    return;
  }

  [super setContentOffset:offset];

  CGFloat scale = UIScreen.mainScreen.scale;  // Points-to-pixels scale.
  float offset_x = offset.x * scale;
  float offset_y = offset.y * scale;
  if (_navItem->content_offset_x == offset_x &&
      _navItem->content_offset_y == offset_y) {
    return;
  }
  _navItem->content_offset_x = offset_x;
  _navItem->content_offset_y = offset_y;
  if (_navItem->callbacks_enabled) {
    _navItem->OnScroll();
  }
}

- (CGSize)contentSize {
  if (_contentsChanged) {
    [self updateContentCache];
  }
  return [super contentSize];
}

- (BOOL)isAccessibilityElement {
  return NO;
}

- (BOOL)shouldUpdateFocusInContext:(UIFocusUpdateContext*)context {
  return YES;
}
@end

namespace starboard {
namespace shared {
namespace uikit {

UiNavItemContainer::UiNavItemContainer(const SbUiNavCallbacks* callbacks,
                                       void* callback_context)
    : SbUiNavItemPrivate(kSbUiNavItemTypeContainer,
                         callbacks,
                         callback_context) {
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      SBDUiNavItemContainer* thisItem =
          [[SBDUiNavItemContainer alloc] initWithNavItem:this];
      // Retain a reference to the Objective-C object.
      uikit_object = (__bridge_retained void*)thisItem;
    });
  }
}

void UiNavItemContainer::Destroy() {
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      // Automatically unregister this content item.
      SetContainer(nullptr);

      // Automatically unregister this item's contents.
      while (!content_items.empty()) {
        content_items.front()->SetContainer(nullptr);
      }

      SBDUiNavItemContainer* thisItem =
          (__bridge_transfer SBDUiNavItemContainer*)uikit_object;
      SB_DCHECK(thisItem.subviews.count == 0);

      // The root view will not have a container, but may still be attached
      // to a SbWindow as its superview. Ensure it is detached.
      if (thisItem.superview) {
        [thisItem removeFromSuperview];
      }

      // Release the reference to the Objective-C object.
      thisItem = nil;
    });
  }
}

void UiNavItemContainer::SetFocusDuration(float seconds) {}

void UiNavItemContainer::SetDir(SbUiNavItemDir dir) {
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      SBDUiNavItemContainer* thisItem =
          (__bridge SBDUiNavItemContainer*)uikit_object;
      if (thisItem.dir.is_left_to_right != dir.is_left_to_right ||
          thisItem.dir.is_top_to_bottom != dir.is_top_to_bottom) {
        // Update directionality and force recalculation of content cache.
        thisItem.dir = dir;
        thisItem.contentsChanged = YES;
      }
    });
  }
}

void UiNavItemContainer::SetSize(float width, float height) {
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      CGFloat scale = UIScreen.mainScreen.scale;  // Points-to-pixels scale.
      SBDUiNavItemContainer* thisItem =
          (__bridge SBDUiNavItemContainer*)uikit_object;
      // Update the bounds size but preserve its origin as that is modified
      // when setting the content offset. This may call setContentOffset as a
      // side effect -- make sure to ignore any changes requested by UIKit.
      {
        ScopedSpinLock scoped_lock(lock);
        ++content_offset_lock_count;
      }
      CGRect newBounds = thisItem.bounds;
      newBounds.size = CGSizeMake(width / scale, height / scale);
      thisItem.bounds = newBounds;
      {
        ScopedSpinLock scoped_lock(lock);
        --content_offset_lock_count;
      }
      if (enabled && container != nullptr) {
        container->UpdateContentCache();
      }
    });
  }
}

void UiNavItemContainer::SetTransform(const SbUiNavMatrix2x3* transform) {
  // Make a copy of the transform since it'll be used in another thread.
  SbUiNavMatrix2x3 transform_copy = *transform;

  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      CGFloat scale = UIScreen.mainScreen.scale;  // Points-to-pixels scale.
      SBDUiNavItemContainer* thisItem =
          (__bridge SBDUiNavItemContainer*)uikit_object;
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

bool UiNavItemContainer::GetFocusTransform(SbUiNavMatrix4* out_transform) {
  return false;
}

bool UiNavItemContainer::GetFocusVector(float* out_x, float* out_y) {
  return false;
}

void UiNavItemContainer::SetContainer(UiNavItemContainer* new_container) {
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

void UiNavItemContainer::RegisterContent(SbUiNavItemPrivate* content) {
  SB_DCHECK([NSThread isMainThread]);
  SB_DCHECK(content != this);
  SB_DCHECK(content->container == nullptr);

  content_items.push_back(content);

  SB_DCHECK(uikit_object);
  SB_DCHECK(content->uikit_object);
  SBDUiNavItemContainer* thisItem =
      (__bridge SBDUiNavItemContainer*)uikit_object;
  UIView* contentItem = (__bridge UIView*)content->uikit_object;
  if (content->type == kSbUiNavItemTypeContainer) {
    // Containers should always be behind focus items.
    [thisItem insertSubview:contentItem atIndex:0];
  } else {
    // Focus items should always be in front of containers.
    [thisItem addSubview:contentItem];
  }
}

void UiNavItemContainer::UnregisterContent(SbUiNavItemPrivate* content) {
  SB_DCHECK([NSThread isMainThread]);
  SB_DCHECK(content != this);
  SB_DCHECK(content->container == this);

  for (auto iter = content_items.begin();; ++iter) {
    if (iter == content_items.end()) {
      // |content_items| is not consistent with |content->container|.
      SB_NOTREACHED();
      return;
    }
    if (*iter == content) {
      content_items.erase(iter);
      break;
    }
  }

  SB_DCHECK(uikit_object);
  SB_DCHECK(content->uikit_object);
  UIView* contentItem = (__bridge UIView*)content->uikit_object;
  [contentItem removeFromSuperview];
}

void UiNavItemContainer::SetContentOffset(float x, float y) {
  // Lock content offset from changes in case a scroll animation is in
  // progress.
  {
    ScopedSpinLock scoped_lock(lock);
    ++content_offset_lock_count;
    content_offset_x = x;
    content_offset_y = y;
  }

  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      // Update the bounds to the current content offset, not the one that
      // was passed to the function, since another thread may have changed the
      // content offset again.
      CGFloat scale = UIScreen.mainScreen.scale;  // Points-to-pixels scale.
      SBDUiNavItemContainer* thisItem =
          (__bridge SBDUiNavItemContainer*)uikit_object;
      [thisItem stopScrollAnimation];
      ScopedSpinLock scoped_lock(lock);
      CGRect newBounds = thisItem.bounds;
      newBounds.origin =
          CGPointMake(content_offset_x / scale, content_offset_y / scale);
      thisItem.bounds = newBounds;
      --content_offset_lock_count;
    });
  }
}

void UiNavItemContainer::GetContentOffset(float* out_x, float* out_y) {
  ScopedSpinLock scoped_lock(lock);
  *out_x = content_offset_x;
  *out_y = content_offset_y;
}

void UiNavItemContainer::FocusContent(SbUiNavItemPrivate* content) {
  SB_DCHECK([NSThread isMainThread]);

  SBDUiNavItemContainer* thisItem =
      (__bridge SBDUiNavItemContainer*)uikit_object;

  // Walk up the hierarchy to find the SBDApplicationWindow if this item is
  // attached to one.
  for (UIView* view = thisItem.superview; view != nil; view = view.superview) {
    if ([view isMemberOfClass:[SBDApplicationWindow class]]) {
      SBDApplicationWindow* applicationWindow = (SBDApplicationWindow*)view;
      [applicationWindow
          setFocus:(__bridge id<UIFocusEnvironment>)content->uikit_object];
      break;
    }
  }
}

void UiNavItemContainer::UpdateContentCache() {
  SB_DCHECK([NSThread isMainThread]);

  // Flag the content cache as dirty for all containers in the hierarchy.
  for (auto current = this; current != nullptr; current = current->container) {
    SBDUiNavItemContainer* currentItem =
        (__bridge SBDUiNavItemContainer*)current->uikit_object;
    if (currentItem.contentsChanged) {
      break;
    }
    currentItem.contentsChanged = YES;
    if (!current->enabled) {
      break;
    }
  }
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
