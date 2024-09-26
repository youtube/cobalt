// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_LEGACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_LEGACY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_highlighting.h"

@protocol TabStripPresentation;
@class ViewRevealingVerticalPanHandler;

// A legacy coordinator that presents the public interface for the tablet tab
// strip feature.
@interface TabStripLegacyCoordinator : ChromeCoordinator<TabStripHighlighting>

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The base view controller for this coordinator. This is required because the
// TabStripLegacyCoordinator is instantiated before the BrowserViewController.
@property(nonatomic, weak, readwrite) UIViewController* baseViewController;

// Provides methods for presenting the tab strip and checking the visibility
// of the tab strip in the containing object.
@property(nonatomic, weak) id<TabStripPresentation> presentationProvider;

// The duration to wait before starting tab strip animations. Used to
// synchronize animations.
@property(nonatomic, assign) NSTimeInterval animationWaitDuration;

// Animatee for this tab strip. It is not added to the `panGestureHandler` as
// it needs to be run in sync with BVC.
@property(nonatomic, readonly, strong) id<ViewRevealingAnimatee> animatee;

// Sets the pan gesture handler for the tab strip controller.
- (void)setPanGestureHandler:
    (ViewRevealingVerticalPanHandler*)panGestureHandler;

// Hides or shows the TabStrip.
- (void)hideTabStrip:(BOOL)hidden;

// Force resizing layout of the tab strip.
- (void)tabStripSizeDidChange;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_LEGACY_COORDINATOR_H_
