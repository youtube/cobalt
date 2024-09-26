// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

class Browser;
@class TabGridCoordinator;

// This delegate is used to drive the TabSwitcher dismissal and execute code
// when the presentation and dismmiss animations finishes.
@protocol TabGridCoordinatorDelegate

// Informs the delegate the tab switcher that the given browser should be set to
// active. If `dismissTabGrid` is YES, the tab grid itself should also be
// dismissed. This should always be the case except when using the thumb strip,
// where the tab grid is never dismissed
- (void)tabGrid:(TabGridCoordinator*)tabGrid
    shouldActivateBrowser:(Browser*)browser
           dismissTabGrid:(BOOL)dismissTabGrid
             focusOmnibox:(BOOL)focusOmnibox;

// Informs the delegate that the tab switcher is done and should be dismissed.
- (void)tabGridDismissTransitionDidEnd:(TabGridCoordinator*)tabGrid;

// Asks the delegate for the page that should currently be active.
- (TabGridPage)activePageForTabGrid:(TabGridCoordinator*)tabGrid;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_COORDINATOR_DELEGATE_H_
