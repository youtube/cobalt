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

#include "starboard/extension/ui_navigation.h"

#import <UIKit/UIKit.h>

#include "starboard/shared/uikit/ui_nav_item_container.h"
#include "starboard/shared/uikit/ui_nav_item_focus.h"
#include "starboard/common/log.h"

#import "starboard/shared/uikit/defines.h"
#import "starboard/shared/uikit/starboard_application.h"
#import "starboard/shared/uikit/window_manager.h"

namespace starboard {
namespace shared {
namespace uikit {

using starboard::ScopedSpinLock;
using starboard::shared::uikit::UiNavItemContainer;
using starboard::shared::uikit::UiNavItemFocus;

SbUiNavItem CreateItem(SbUiNavItemType type,
                       const SbUiNavCallbacks* callbacks,
                       void* callback_context) {
  switch (type) {
    case kSbUiNavItemTypeFocus:
      return new UiNavItemFocus(callbacks, callback_context);
      break;
    case kSbUiNavItemTypeContainer:
      return new UiNavItemContainer(callbacks, callback_context);
      break;
    default:
      SB_NOTREACHED();
  }
}

void DestroyItem(SbUiNavItem item) {
  item->Destroy();
}

void SetFocus(SbUiNavItem item) {
  if (item == kSbUiNavItemInvalid) {
    // Force focus off any SbUiNavItem and onto the main window.
    @autoreleasepool {
      onApplicationMainThreadAsync(^{
        SBDWindowManager* windowManager = SBDGetApplication().windowManager;
        [windowManager.currentApplicationWindow setFocus:nil];
      });
    }
    return;
  }

  SB_DCHECK(item && item->type == kSbUiNavItemTypeFocus);
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      UiNavItemContainer* root = item->container;
      if (!root) {
        // This hasn't been registered as a content of anything.
        return;
      }
      while (root->container) {
        root = root->container;
      }
      root->FocusContent(item);
    });
  }
}

void SetItemEnabled(SbUiNavItem item, bool enabled) {
  // Ensure that callbacks are enabled / disabled by the time this function
  // returns. The navigation object can be enabled / disabled asynchronously.
  {
    ScopedSpinLock scoped_lock(item->lock);
    item->callbacks_enabled = enabled;
  }

  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      if (item->enabled == enabled) {
        return;
      }
      item->enabled = enabled;
      if (item->container) {
        item->container->UpdateContentCache();
      }
    });
  }
}

void SetItemFocusDuration(SbUiNavItem item, float seconds) {
  item->SetFocusDuration(seconds);
}

void SetItemDir(SbUiNavItem item, SbUiNavItemDir dir) {
  item->SetDir(dir);
}

void SetItemSize(SbUiNavItem item, float width, float height) {
  item->SetSize(width, height);
}

void SetItemTransform(SbUiNavItem item, const SbUiNavMatrix2x3* transform) {
  item->SetTransform(transform);
}

bool GetItemFocusTransform(SbUiNavItem item, SbUiNavMatrix4* out_transform) {
  return item->GetFocusTransform(out_transform);
}

bool GetItemFocusVector(SbUiNavItem item, float* out_x, float* out_y) {
  return item->GetFocusVector(out_x, out_y);
}

void SetItemContainerWindow(SbUiNavItem item, SbWindow window) {
  SB_DCHECK(item->type == kSbUiNavItemTypeContainer &&
            item->container == nullptr);

  // Attach the root item to the window.
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      UIView* rootView = (__bridge UIView*)item->uikit_object;
      [rootView removeFromSuperview];
      if (SbWindowIsValid(window)) {
        SBDWindowManager* windowManager = SBDGetApplication().windowManager;
        SBDApplicationWindow* applicationWindow =
            [windowManager applicationWindowForStarboardWindow:window];
        [applicationWindow attachUiNavigationView:rootView];
      }
    });
  }
}

void SetItemContainerItem(SbUiNavItem item, SbUiNavItem container) {
  UiNavItemContainer* container_item =
      static_cast<UiNavItemContainer*>(container);
  @autoreleasepool {
    onApplicationMainThreadAsync(^{
      if (item->container == container_item) {
        return;
      }
      if (item->container != nullptr && container_item != nullptr) {
        item->SetContainer(nullptr);
      }
      item->SetContainer(container_item);
    });
  }
}

void SetItemContentOffset(SbUiNavItem item,
                          float content_offset_x,
                          float content_offset_y) {
  item->SetContentOffset(content_offset_x, content_offset_y);
}

void GetItemContentOffset(SbUiNavItem item,
                          float* out_content_offset_x,
                          float* out_content_offset_y) {
  item->GetContentOffset(out_content_offset_x, out_content_offset_y);
}

void DoBatchUpdate(void (*update_function)(void*), void* context) {
  @autoreleasepool {
    // Execute synchronously on the UI thread.
    onApplicationMainThread(^{
      update_function(context);
    });
  }
}

const SbUiNavInterface kUiNavInterface = {
  kCobaltExtensionUiNavigationName,
  1,
  &CreateItem,
  &DestroyItem,
  &SetFocus,
  &SetItemEnabled,
  &SetItemDir,
  &SetItemFocusDuration,
  &SetItemSize,
  &SetItemTransform,
  &GetItemFocusTransform,
  &GetItemFocusVector,
  &SetItemContainerWindow,
  &SetItemContainerItem,
  &SetItemContentOffset,
  &GetItemContentOffset,
  &DoBatchUpdate
};

const void* GetUINavigationApi() {
  return &kUiNavInterface;
}

} // namespace uikit
} // namespace shared
} // namespace starboard
