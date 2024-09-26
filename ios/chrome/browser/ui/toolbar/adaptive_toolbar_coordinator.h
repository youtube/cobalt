// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/toolbar/public/side_swipe_toolbar_snapshot_providing.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinatee.h"

@class AdaptiveToolbarViewController;
class Browser;
@class LocationBarCoordinator;

// Coordinator for the adaptive toolbar. This Coordinator is the super class of
// the specific coordinator (primary or secondary).
@interface AdaptiveToolbarCoordinator
    : ChromeCoordinator<SideSwipeToolbarSnapshotProviding, ToolbarCoordinatee>

// The Toolbar view controller owned by this coordinator.
@property(nonatomic, strong) AdaptiveToolbarViewController* viewController;

// A reference to the location bar coordinator.
@property(nonatomic, weak) LocationBarCoordinator* locationBarCoordinator;

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Whether the omnibox is currently the first responder.
- (BOOL)isOmniboxFirstResponder;
// Whether the omnibox popup is currently presented.
- (BOOL)showingOmniboxPopup;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_COORDINATOR_H_
