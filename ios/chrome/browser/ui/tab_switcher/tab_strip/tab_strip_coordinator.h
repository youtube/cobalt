// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
@protocol TabStripContaining;
@class TabStripViewController;

// Coordinator for the tab strip.
@interface TabStripCoordinator : ChromeCoordinator

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The TabStrip view controller owned by this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// The TabStrip view owned by the viewcontroller of this coordinator.
@property(nonatomic, strong, readonly) UIView<TabStripContaining>* view;

// Hides or shows the tab strip.
- (void)hideTabStrip:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_COORDINATOR_H_
