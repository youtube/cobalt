// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_CREATE_OR_EDIT_TAB_GROUP_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_CREATE_OR_EDIT_TAB_GROUP_VIEW_CONTROLLER_DELEGATE_H_

@class CreateTabGroupViewController;

// Delegate protocol for `CreateTabGroupViewController`.
@protocol CreateOrEditTabGroupViewControllerDelegate

// Called when the user dismissed the `CreateTabGroupViewController`.
- (void)createOrEditTabGroupViewControllerDidDismiss:
            (CreateTabGroupViewController*)viewController
                                            animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_CREATE_OR_EDIT_TAB_GROUP_VIEW_CONTROLLER_DELEGATE_H_
