// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class TabGroupIndicatorView;
@protocol ToolbarHeightDelegate;

// Coordinator for the tab group indicator.
@interface TabGroupIndicatorCoordinator : ChromeCoordinator

// A read-only reference to the TabGroupIndicatorView instance.
// This coordinator does not manage a view controller, but the view is
// assigned to the PrimaryToolbarCoordinator's viewController.
@property(nonatomic, strong, readonly) TabGroupIndicatorView* view;

// This property ensures the FacePile view controller is added as a child of the
// correct parent view controller within the hierarchy. This is necessary to
// avoid UIViewControllerHierarchyInconsistency exceptions. Specifically, it
// should be added to the same view controller whose view contains the
// TabGroupIndicatorView.
@property(nonatomic, weak) UIViewController* parentViewController;

/// Delegate that handles the toolbars height.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Whether the current page is an NTP.
@property(nonatomic, assign) BOOL displayedOnNTP;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_COORDINATOR_H_
