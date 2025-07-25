// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_configuring.h"

namespace web {
class WebState;
}

@class BubblePresenter;
@protocol NewTabPageComponentFactoryProtocol;
@protocol NewTabPageControllerDelegate;

// Coordinator handling the NTP.
@interface NewTabPageCoordinator : ChromeCoordinator <NewTabPageConfiguring>

// Initializes this coordinator with its `browser`, a nil base view
// controller, and the given `componentFactory`.
- (instancetype)initWithBrowser:(Browser*)browser
               componentFactory:
                   (id<NewTabPageComponentFactoryProtocol>)componentFactory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The base view controller for this coordinator
@property(weak, nonatomic, readwrite) UIViewController* baseViewController;

// ViewController associated with this coordinator.
@property(nonatomic, readonly) UIViewController* viewController;

// Delete for NTP and it's subclasses to communicate with the toolbar.
@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;

// Returns `YES` if the coordinator is started.
@property(nonatomic, readonly) BOOL started;

// Bubble presenter for displaying IPH bubbles relating to the NTP.
@property(nonatomic, strong) BubblePresenter* bubblePresenter;

// Currently selected feed.
@property(nonatomic, readonly) FeedType selectedFeed;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// Stop any scrolling in the scroll view.
- (void)stopScrolling;

// Whether the NTP is scrolled to the top.
- (BOOL)isScrolledToTop;

// Reloads the content of the NewTabPage. Does not do anything on Incognito.
- (void)reload;

// Called when the user navigates to the NTP.
- (void)didNavigateToNTPInWebState:(web::WebState*)webState;

// Called when the user navigates away from the NTP.
- (void)didNavigateAwayFromNTP;

// The location bar will lose focus.
- (void)locationBarWillResignFirstResponder;

// The location bar has lost focus.
- (void)locationBarDidResignFirstResponder;

// Tell location bar has taken focus.
- (void)locationBarDidBecomeFirstResponder;

// Constrains the named layout guide for the Discover header menu button.
- (void)constrainDiscoverHeaderMenuButtonNamedGuide;

// Updates the new tab page based on if there is unseen content in the Following
// feed.
- (void)updateFollowingFeedHasUnseenContent:(BOOL)hasUnseenContent;

// Called when the given `feedType` has completed updates.
- (void)handleFeedModelDidEndUpdates:(FeedType)feedType;

// Checks if there are any WebStates showing an NTP at this time. If not, then
// stops the NTP.
- (void)stopIfNeeded;

// Checks if NTP is active for the current webState.
- (BOOL)isNTPActiveForCurrentWebState;

// Returns YES if the fakebox is pinned or scrolled to the top.
- (BOOL)isFakeboxPinned;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_
