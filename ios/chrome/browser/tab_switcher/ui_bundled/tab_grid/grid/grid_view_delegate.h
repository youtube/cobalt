// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GRID_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GRID_VIEW_DELEGATE_H_

// Delegate protocol to relay user interactions from a grid UI.
@protocol GridViewDelegate <NSObject>

// Notifies if the header is visible or not.
- (void)gridViewHeaderHidden:(BOOL)hidden;

// Shows the recent activity half sheet of a shared tab group.
- (void)showRecentActivity;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_GRID_VIEW_DELEGATE_H_
