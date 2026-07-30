// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_PINNED_TABS_PINNED_CELL_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_PINNED_TABS_PINNED_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_context_menu/tab_cell.h"

namespace web {
class WebStateID;
}  // namespace web

// A cell for the pinned tabs view. Contains an icon, title, snapshot.
@interface PinnedCell : TabCell

// Returns a transition selection cell with the same frame as `cell`, but with
// no visible content view, no delegate, and no identifier.
//
// Note: Transition selection cell is a kind of "copy" of a PinnedCell to be
// used in the animated transitions that only shows selection state (that is,
// its content view is hidden).
+ (instancetype)transitionSelectionCellFromCell:(PinnedCell*)cell;

// Settable UI elements of the cell.
@property(nonatomic, strong) UIImage* icon;
@property(nonatomic, strong) UIImage* snapshot;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) web::WebStateID pinnedItemIdentifier;

// Starts the activity indicator animation.
- (void)showActivityIndicator;
// Stops the activity indicator animation.
- (void)hideActivityIndicator;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_PINNED_TABS_PINNED_CELL_H_
