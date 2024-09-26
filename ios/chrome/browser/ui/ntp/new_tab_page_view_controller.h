// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_supporting.h"

@class BubblePresenter;
@class ContentSuggestionsViewController;
@class FeedHeaderViewController;
@class FeedMetricsRecorder;
@class FeedWrapperViewController;
@protocol NewTabPageContentDelegate;
@class NewTabPageHeaderViewController;
@protocol OverscrollActionsControllerDelegate;
@class ViewRevealingVerticalPanHandler;

// View controller containing all the content presented on a standard,
// non-incognito new tab page.
@interface NewTabPageViewController
    : UIViewController <ThumbStripSupporting,
                        NewTabPageConsumer,
                        NewTabPageHeaderViewControllerDelegate,
                        UIScrollViewDelegate>

// View controller wrapping the feed.
@property(nonatomic, strong)
    FeedWrapperViewController* feedWrapperViewController;

// Delegate for the overscroll actions.
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollDelegate;

// The NTP header, containing the fake omnibox and the doodle.
@property(nonatomic, weak) NewTabPageHeaderViewController* headerViewController;

// Delegate for actions relating to the NTP content.
@property(nonatomic, weak) id<NewTabPageContentDelegate> ntpContentDelegate;

// The pan gesture handler to notify of scroll events happening in this view
// controller.
@property(nonatomic, weak) ViewRevealingVerticalPanHandler* panGestureHandler;

// The view controller representing the content suggestions.
@property(nonatomic, strong)
    ContentSuggestionsViewController* contentSuggestionsViewController;

// Feed metrics recorder.
@property(nonatomic, strong) FeedMetricsRecorder* feedMetricsRecorder;

// Whether or not the feed is visible.
@property(nonatomic, assign, getter=isFeedVisible) BOOL feedVisible;

// The view controller representing the NTP feed header.
@property(nonatomic, weak) FeedHeaderViewController* feedHeaderViewController;

// The view controller representing the Feed top section (between the feed
// header and the feed collection).
@property(nonatomic, weak) UIViewController* feedTopSectionViewController;

// Bubble presenter for displaying IPH bubbles relating to the NTP.
@property(nonatomic, strong) BubblePresenter* bubblePresenter;

// Whether or not this NTP has fully appeared for the first time yet. This value
// remains YES if viewDidAppear has been called.
@property(nonatomic, assign) BOOL viewDidAppear;

// Whether the NTP should initially be scrolled into the feed.
@property(nonatomic, assign) BOOL shouldScrollIntoFeed;

// `YES` when notifications indicate the omnibox is focused.
@property(nonatomic, assign) BOOL omniboxFocused;

// `YES` if the omnibox should be focused on when the view appears for voice
// over.
@property(nonatomic, assign) BOOL focusAccessibilityOmniboxWhenViewAppears;

// Initializes the new tab page view controller.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Indicates to the receiver to update its state to focus the omnibox.
- (void)focusOmnibox;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// Stops scrolling in the scroll view.
- (void)stopScrolling;

// Lays out content above feed and adjusts content suggestions.
- (void)updateNTPLayout;

// Signal to the ViewController that the height about the feed needs to be
// recalculated and thus also likely needs to be scrolled up to accommodate for
// the new height. Nothing may happen if the ViewController determines that the
// current scroll state should not change.
- (void)updateHeightAboveFeedAndScrollToTopIfNeeded;

// Returns whether the NTP is scrolled to the top or not.
- (BOOL)isNTPScrolledToTop;

// Lays out and re-configures the NTP content after changing the containing
// collection view, such as when changing feeds.
- (void)layoutContentInParentCollectionView;

// Resets hierarchy of views and view controllers.
- (void)resetViewHierarchy;

// Resets any relevant NTP states due for a content reload.
- (void)resetStateUponReload;

// Sets the NTP collection view's scroll position to `contentOffset`, unless it
// is beyond the top of the feed. In that case, sets the scroll position to the
// top of the feed.
- (void)setContentOffsetToTopOfFeed:(CGFloat)contentOffset;

// Checks the content size of the feed and updates the bottom content inset to
// ensure the feed is still scrollable to the minimum height.
- (void)updateFeedInsetsForMinimumHeight;

// Updates the scroll position to account for the feed promo being removed.
- (void)updateScrollPositionForFeedTopSectionClosed;

// Signals that the feed has completed its updates (i.e. loading cards).
- (void)feedLayoutDidEndUpdates;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_CONTROLLER_H_
