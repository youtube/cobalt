// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"

#import "base/check.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_omnibox_positioning.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Animation time for the shift up/down animations to focus/defocus omnibox.
const CGFloat kShiftTilesDownAnimationDuration = 0.2;
const CGFloat kShiftTilesUpAnimationDuration = 0.1;
}  // namespace

@interface NewTabPageViewController () <NewTabPageOmniboxPositioning,
                                        UICollectionViewDelegate,
                                        UIGestureRecognizerDelegate>

// The overscroll actions controller managing accelerators over the toolbar.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;

// Whether or not the user has scrolled into the feed, transferring ownership of
// the omnibox to allow it to stick to the top of the NTP.
// With Web Channels enabled, also determines if the feed header is stuck to the
// top.
// TODO(crbug.com/1277504): Modify this comment when Web Channels is released.
@property(nonatomic, assign, getter=isScrolledIntoFeed) BOOL scrolledIntoFeed;

// Whether or not the fake omnibox is pineed to the top of the NTP.
@property(nonatomic, assign) BOOL fakeOmniboxPinnedToTop;

// Array of constraints used to pin the fake Omnibox header into the top of the
// view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* fakeOmniboxConstraints;
// Constraint that pins the fake Omnibox to the top of the view. A subset of
// `fakeOmniboxConstraints`.
@property(nonatomic, strong) NSLayoutConstraint* headerTopAnchor;

// Array of constraints used to pin the feed header to the top of the NTP. Only
// applicable with Web Channels enabled.
// TODO(crbug.com/1277504): Modify this comment when Web Channels is released.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* feedHeaderConstraints;

// `YES` if the NTP starting content offset should be set to a previous scroll
// state (when navigating away and back), and `NO` if it should be the top of
// the NTP.
@property(nonatomic, assign) BOOL hasSavedOffsetFromPreviousScrollState;

// The content offset saved from a previous scroll state in the NTP. If this is
// set, `hasSavedOffsetFromPreviousScrollState` should be YES.
@property(nonatomic, assign) CGFloat savedScrollOffset;

// The scroll position when a scrolling event starts.
@property(nonatomic, assign) int scrollStartPosition;

// Whether the omnibox should be focused once the collection view appears.
@property(nonatomic, assign) BOOL shouldFocusFakebox;

// Array of all view controllers above the feed.
@property(nonatomic, strong)
    NSMutableArray<UIViewController*>* viewControllersAboveFeed;

// Identity disc shown in the NTP.
// TODO(crbug.com/1170995): Remove once the Feed header properly supports
// ContentSuggestions.
@property(nonatomic, weak) UIButton* identityDiscButton;

// Tap gesture recognizer when the omnibox is focused.
@property(nonatomic, strong) UITapGestureRecognizer* tapGestureRecognizer;

// Animator for the `shiftTilesUpToFocusOmnibox` animation.
@property(nonatomic, strong) UIViewPropertyAnimator* animator;

// When the omnibox is focused, this value represents the shift distance of the
// collection needed to pin the omnibox to the top. It is 0 if the omnibox has
// not been moved when focused (i.e. the collection was already scrolled to
// top).
@property(nonatomic, assign, readwrite) CGFloat collectionShiftingOffset;

// `YES` if the collection is scrolled to the point where the omnibox is stuck
// to the top of the NTP. Used to lock this position in place on various frame
// changes.
@property(nonatomic, assign, readwrite) BOOL scrolledToMinimumHeight;

// The added y-offset of the NTP collection view to make up for the header.
// Without this, the offset is negative at the top of the NTP.
@property(nonatomic, assign) CGFloat additionalOffset;

// If YES the animations of the fake omnibox triggered when the collection is
// scrolled (expansion) are disabled. This is used for the fake omnibox focus
// animations so the constraints aren't changed while the ntp is scrolled.
@property(nonatomic, assign) BOOL disableScrollAnimation;

// `YES` if the fakebox header should be animated on scroll.
@property(nonatomic, assign) BOOL shouldAnimateHeader;

// Keeps track of how long the shift down animation has taken. Used to update
// the Content Suggestions header as the animation progresses.
@property(nonatomic, assign) CFTimeInterval shiftTileStartTime;

// YES if `-viewDidLoad:` has finished executing. This is used to ensure that
// constraints are not set before the views have been added to view hierarchy.
@property(nonatomic, assign) BOOL viewDidFinishLoading;

// YES if the NTP is in the middle of animating an omnibox focus.
@property(nonatomic, assign) BOOL isAnimatingOmniboxFocus;

@end

@implementation NewTabPageViewController

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _viewControllersAboveFeed = [[NSMutableArray alloc] init];

    _tapGestureRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(unfocusOmnibox)];

    _collectionShiftingOffset = 0;
    _additionalOffset = 0;
    _shouldAnimateHeader = YES;
    _focusAccessibilityOmniboxWhenViewAppears = YES;
  }
  return self;
}

- (void)dealloc {
  _viewControllersAboveFeed = nil;
  [self.overscrollActionsController invalidate];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  DCHECK(self.feedWrapperViewController);
  DCHECK(self.contentSuggestionsViewController);

  // TODO(crbug.com/1262536): Remove this when bug is fixed.
  [self.feedWrapperViewController loadViewIfNeeded];
  [self.contentSuggestionsViewController loadViewIfNeeded];

  // Prevent the NTP from spilling behind the toolbar and tab strip.
  self.view.clipsToBounds = YES;

  // TODO(crbug.com/1403612): The contentCollectionView width might be narrower
  // than the ContentSuggestions view. This causes elements to be hidden. A
  // gesture recognizer is added to allow these elements to be interactable.
  UITapGestureRecognizer* singleTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleSingleTapInView:)];
  singleTapRecognizer.delegate = self;
  [self.view addGestureRecognizer:singleTapRecognizer];
  self.view.backgroundColor = ntp_home::NTPBackgroundColor();

  [self registerNotifications];

  [self layoutContentInParentCollectionView];

  self.identityDiscButton = [self.headerViewController identityDiscButton];
  DCHECK(self.identityDiscButton);

  self.viewDidFinishLoading = YES;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  self.headerViewController.showing = YES;

  [self updateNTPLayout];

  // Scroll to the top before coming into view to minimize sudden visual jerking
  // for startup instances showing the NTP.
  if (!self.viewDidAppear) {
    [self setContentOffsetToTop];
  }

  if (self.focusAccessibilityOmniboxWhenViewAppears && !self.omniboxFocused) {
    [self.headerViewController focusAccessibilityOnOmnibox];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // `-feedLayoutDidEndUpdates` handles the need to either scroll to the top of
  // go back to a previous scroll state when the feed is enabled. This handles
  // the instance when the feed is not enabled.
  // `-viewWillAppear:` is not the suitable place for this as long as the user
  // can open a new tab while an NTP is currently visible. `-viewWillAppear:` is
  // called before the offset can be saved, so `-setContentOffsetToTop` will
  // reset any scrolled position.
  // It is NOT safe to reset `hasSavedOffsetFromPreviousScrollState` to NO here
  // because -updateHeightAboveFeedAndScrollToTopIfNeeded calls from async
  // updates to the Content Suggestions (i.e. MVT, Doodle) can happen after
  // this.
  if (!self.feedVisible) {
    if (self.hasSavedOffsetFromPreviousScrollState) {
      [self setContentOffset:self.savedScrollOffset];
    } else {
      [self setContentOffsetToTop];
    }
  }

  // Updates omnibox to ensure that the dimensions are correct when navigating
  // back to the NTP.
  [self updateFakeOmniboxForScrollPosition];

  if (self.shouldFocusFakebox) {
    [self shiftTilesUpToFocusOmnibox];
    self.shouldFocusFakebox = NO;
  }

  if (self.isFeedVisible) {
    [self updateFeedInsetsForMinimumHeight];
  } else {
    [self setMinimumHeight];
  }

  [self.bubblePresenter presentDiscoverFeedHeaderTipBubble];

  // Scrolls NTP into feed initially if `shouldScrollIntoFeed`.
  if (self.shouldScrollIntoFeed) {
    [self scrollIntoFeed];
    self.shouldScrollIntoFeed = NO;
  }

  [self updateFeedSigninPromoIsVisible];

  // Since this VC is shared across web states, the stickiness might have
  // changed in another tab. This ensures that the sticky elements are correct
  // whenever an NTP reappears.
  [self handleStickyElementsForScrollPosition:[self scrollPosition] force:YES];

  self.viewDidAppear = YES;
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  self.headerViewController.showing = NO;
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];

  if (!self.viewDidAppear) {
    // The native views in the NTP are not top anchored to the surface in any
    // way. They are stacked on top of the top of the Feed contents, and the top
    // Safe Area insets are factored in the height needed above the feed in
    // -updateFeedInsetsForContentAbove. Update that height here as it is the
    // soonest place the Safe Area insets are ready.
    [self updateHeightAboveFeedAndScrollToTopIfNeeded];
  }

  [self.headerViewController updateConstraints];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak NewTabPageViewController* weakSelf = self;

  CGFloat yOffsetBeforeRotation = [self scrollPosition];
  CGFloat heightAboveFeedBeforeRotation = [self heightAboveFeed];

  void (^alongsideBlock)(id<UIViewControllerTransitionCoordinatorContext>) = ^(
      id<UIViewControllerTransitionCoordinatorContext> context) {
    [weakSelf handleStickyElementsForScrollPosition:[weakSelf scrollPosition]
                                              force:YES];

    // Redraw the ContentSuggestionsViewController to properly
    // caclculate the new adjustedContentSuggestionsHeight value.
    // TODO(crbug.com/1170995): Remove once the Feed supports a custom
    // header.
    [[weakSelf contentSuggestionsViewController].view setNeedsLayout];
    [[weakSelf contentSuggestionsViewController].view layoutIfNeeded];

    CGFloat heightAboveFeedDifference =
        [weakSelf heightAboveFeed] - heightAboveFeedBeforeRotation;

    // Rotating the device can change the content suggestions height. This
    // ensures that it is adjusted if necessary.
    if (yOffsetBeforeRotation < 0) {
      weakSelf.collectionView.contentOffset =
          CGPointMake(0, yOffsetBeforeRotation - heightAboveFeedDifference);
      [weakSelf updateNTPLayout];
    }
    [weakSelf.view setNeedsLayout];
    [weakSelf.view layoutIfNeeded];

    // Pinned offset is different based on the orientation, so we reevaluate the
    // minimum scroll position upon device rotation.
    CGFloat pinnedOffsetY = [weakSelf pinnedOffsetY];
    if (weakSelf.omniboxFocused && [weakSelf scrollPosition] < pinnedOffsetY) {
      weakSelf.collectionView.contentOffset = CGPointMake(0, pinnedOffsetY);
    }
    if (!weakSelf.isFeedVisible) {
      [weakSelf setMinimumHeight];
    }
  };
  [coordinator
      animateAlongsideTransition:alongsideBlock
                      completion:^(
                          id<UIViewControllerTransitionCoordinatorContext>) {
                        [self updateNTPLayout];
                        if (self.isFeedVisible) {
                          [self updateFeedInsetsForMinimumHeight];
                        }
                      }];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass) {
    // Update header constant to cover rotation instances. When the omnibox is
    // pinned to the top, the fake omnibox is the one shown only in portrait
    // mode, so if the NTP is opened in landscape mode, a rotation to portrait
    // mode needs to update the top anchor constant based on the correct header
    // height.
    self.headerTopAnchor.constant =
        -([self stickyOmniboxHeight] + [self feedHeaderHeight]);
    [self.contentSuggestionsViewController.view setNeedsLayout];
    [self.contentSuggestionsViewController.view layoutIfNeeded];
    [self.ntpContentDelegate reloadContentSuggestions];
  }

  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self updateFakeOmniboxForScrollPosition];
  }

  [self.headerViewController updateConstraints];
  [self updateOverscrollActionsState];
}

#pragma mark - Public

- (void)focusOmnibox {
  // Do nothing if the omnibox is already focused or is in the middle of a
  // focus. This prevents `collectionShiftingOffset` from being reset to close
  // to 0, which would result in the defocus animation not returning to the top
  // of the NTP if that was the original position.
  // This is relevant beacuse the omnibox logic signals the NTP to focus the
  // omnibox when it becomes the keyboard first responder, but that happens
  // during the NTP focus animation, which results in -focusOmnibox being called
  // twice.
  if (self.omniboxFocused || self.isAnimatingOmniboxFocus) {
    return;
  }

  // If the feed is meant to be visible and its contents have not loaded yet,
  // then any omnibox focus animations (i.e. opening app from search widget
  // action) needs to wait until it is ready. viewDidAppear: currently serves as
  // this proxy as there is no specific signal given from the feed that its
  // contents have loaded.
  if (self.isFeedVisible && !self.viewDidAppear) {
    self.shouldFocusFakebox = YES;
  } else {
    [self shiftTilesUpToFocusOmnibox];
  }
}

- (void)layoutContentInParentCollectionView {
  DCHECK(self.feedWrapperViewController);
  DCHECK(self.contentSuggestionsViewController);

  // Ensure the view is loaded so we can set the accessibility identifier.
  [self.feedWrapperViewController loadViewIfNeeded];
  self.collectionView.accessibilityIdentifier = kNTPCollectionViewIdentifier;

  // Configures the feed and wrapper in the view hierarchy.
  UIView* feedView = self.feedWrapperViewController.view;
  [self.feedWrapperViewController willMoveToParentViewController:self];
  [self addChildViewController:self.feedWrapperViewController];
  [self.view addSubview:feedView];
  [self.feedWrapperViewController didMoveToParentViewController:self];
  feedView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(feedView, self.view);

  // Configures the content suggestions in the view hierarchy.
  // TODO(crbug.com/1262536): Remove this when issue is fixed.
  if (self.contentSuggestionsViewController.parentViewController) {
    [self.contentSuggestionsViewController willMoveToParentViewController:nil];
    [self.contentSuggestionsViewController.view removeFromSuperview];
    [self.contentSuggestionsViewController removeFromParentViewController];
    [self.feedMetricsRecorder
        recordBrokenNTPHierarchy:BrokenNTPHierarchyRelationship::
                                     kContentSuggestionsReset];
  }

  [self addViewControllerAboveFeed:self.contentSuggestionsViewController];

  // Adds the feed top section to the view hierarchy if it exists.
  if (IsDiscoverFeedTopSyncPromoEnabled() &&
      self.feedTopSectionViewController) {
    [self addViewControllerAboveFeed:self.feedTopSectionViewController];
  }

  // Configures the feed header in the view hierarchy if it is visible. Add it
  // in the order that guarantees it is behind `headerViewController` and in
  // front of all other views.
  if (self.feedHeaderViewController) {
    [self addViewControllerAboveFeed:self.feedHeaderViewController];
  }

  [self addViewControllerAboveFeed:self.headerViewController];

  DCHECK(
      [self.headerViewController.view isDescendantOfView:self.containerView]);
  self.headerViewController.view.translatesAutoresizingMaskIntoConstraints = NO;

  // TODO(crbug.com/1170995): The contentCollectionView width might be
  // narrower than the ContentSuggestions view. This causes elements to be
  // hidden, so we set clipsToBounds to ensure that they remain visible. The
  // collection view changes, so we must set this property each time it does.
  self.collectionView.clipsToBounds = NO;

  [self.overscrollActionsController invalidate];
  [self configureOverscrollActionsController];

  // Update NTP collection view constraints to ensure the layout adapts to
  // changes in feed visibility.
  [self applyCollectionViewConstraints];

  // Force relayout so that the views added in this method are rendered ASAP,
  // ensuring it is showing in the new tab animation.
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];

  // If the feed is not visible, we control the delegate ourself (since it is
  // otherwise controlled by the feed service).
  if (!self.isFeedVisible) {
    self.feedWrapperViewController.contentCollectionView.delegate = self;
    [self setMinimumHeight];
  }
}

- (void)willUpdateSnapshot {
  [self.overscrollActionsController clear];
}

- (void)stopScrolling {
  UIScrollView* scrollView = self.collectionView;
  [scrollView setContentOffset:scrollView.contentOffset animated:NO];
}

- (BOOL)isNTPScrolledToTop {
  return [self scrollPosition] <= -[self heightAboveFeed];
}

- (void)updateNTPLayout {
  [self updateFeedInsetsForContentAbove];
  if (self.feedVisible) {
    [self updateFeedInsetsForMinimumHeight];
  }

  // Reload data to ensure the Most Visited tiles and fake omnibox are correctly
  // positioned, in particular during a rotation while a ViewController is
  // presented in front of the NTP.
  [self updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  // Ensure initial fake omnibox layout.
  [self updateFakeOmniboxForScrollPosition];
}

- (void)updateHeightAboveFeedAndScrollToTopIfNeeded {
  if (self.viewDidFinishLoading &&
      !self.hasSavedOffsetFromPreviousScrollState) {
    // Do not scroll to the top if there is a saved scroll state. Also,
    // `-setContentOffsetToTop` potentially updates constaints, and if
    // viewDidLoad has not finished, some views may not in the view hierarchy
    // yet.
    [self updateFeedInsetsForContentAbove];
    [self setContentOffsetToTop];
  }
}

- (void)resetViewHierarchy {
  [self removeFromViewHierarchy:self.feedWrapperViewController];
  [self removeFromViewHierarchy:self.contentSuggestionsViewController];

  for (UIViewController* viewController in self.viewControllersAboveFeed) {
    [self removeFromViewHierarchy:viewController];
  }
  [self.viewControllersAboveFeed removeAllObjects];
}

- (void)resetStateUponReload {
  self.hasSavedOffsetFromPreviousScrollState = NO;
}

- (void)setContentOffsetToTopOfFeed:(CGFloat)contentOffset {
  if (contentOffset < [self offsetWhenScrolledIntoFeed]) {
    [self setContentOffset:contentOffset];
  } else {
    [self scrollIntoFeed];
  }
}

- (void)updateFeedInsetsForMinimumHeight {
  DCHECK(self.isFeedVisible);
  CGFloat minimumNTPHeight =
      self.collectionView.bounds.size.height +
      self.feedWrapperViewController.view.safeAreaInsets.top;
  minimumNTPHeight -= [self feedHeaderHeight];
  if ([self shouldPinFakeOmnibox]) {
    minimumNTPHeight -= ([self.headerViewController headerHeight] +
                         ntp_header::kScrolledToTopOmniboxBottomMargin);
  }

  if (self.collectionView.contentSize.height > minimumNTPHeight) {
    self.collectionView.contentInset =
        UIEdgeInsetsMake(self.collectionView.contentInset.top, 0, 0, 0);
  } else {
    CGFloat bottomInset =
        minimumNTPHeight - self.collectionView.contentSize.height;
    self.collectionView.contentInset = UIEdgeInsetsMake(
        self.collectionView.contentInset.top, 0, bottomInset, 0);
  }
}

- (void)updateScrollPositionForFeedTopSectionClosed {
  if (self.fakeOmniboxPinnedToTop) {
    [self setContentOffset:[self scrollPosition] + [self feedTopSectionHeight]];
  }
}

- (void)feedLayoutDidEndUpdates {
  [self updateFeedInsetsForMinimumHeight];
  // Updating insets can influence contentOffset, so update saved scroll state
  // after it. This handles what the starting offset be with the feed enabled,
  // `-viewWillAppear:` handles when the feed is not enabled.
  // It is NOT safe to reset `hasSavedOffsetFromPreviousScrollState` to NO here
  // because -updateHeightAboveFeedAndScrollToTopIfNeeded calls from async
  // updates to the Content Suggestions (i.e. MVT, Doodle) can happen after
  // this.
  if (self.hasSavedOffsetFromPreviousScrollState) {
    [self setContentOffset:self.savedScrollOffset];
  }
}

#pragma mark - NewTabPageConsumer

- (void)setSavedContentOffset:(CGFloat)offset {
  self.hasSavedOffsetFromPreviousScrollState = YES;
  self.savedScrollOffset = offset;
  [self setContentOffset:offset];
}

- (void)setContentOffsetToTop {
  // There are many instances during NTP startup where the NTP layout is reset
  // (e.g. calling -updateNTPLayout), which involves resetting the scroll
  // offset. Some come from mutliple layout calls from the BVC, some come from
  // an ambifuous source (likely the Feed). Particularly, the mediator's
  // -setContentOffsetForWebState: call happens late in the cycle, which can
  // clash with an already focused omnibox state. That call to reset the content
  // offset to the top is important since the MVTiles and Google doodle are aync
  // fetched/displayed, thus needed a reset. However, in the instance where the
  // omnibox is focused, it is more important to keep that focused state and not
  // show a "double" omibox state.
  // TODO(crbug.com/1371261): Replace the -setContentOffsetForWebState: call
  // with calls directly from all async updates to the NTP.
  if (self.omniboxFocused) {
    return;
  }
  [self setContentOffset:-[self heightAboveFeed]];
  // TODO(crbug.com/1406940): Constraint updating should not be necessary since
  // scrollViewDidScroll: calls this if needed.
  [self setInitialFakeOmniboxConstraints];
  if ([self.ntpContentDelegate isContentHeaderSticky]) {
    [self setInitialFeedHeaderConstraints];
  }
  // Reset here since none of the view lifecycle callbacks (e.g.
  // viewDidDisappear) can be reliably used (it seems) (i.e. switching between
  // NTPs where there is saved scroll state in the destination tab). If the
  // content offset is being set to the top, it is safe to assume this can be
  // set to NO. Being called before setSavedContentOffset: is no problem since
  // then it will be subsequently overriden to YES.
  self.hasSavedOffsetFromPreviousScrollState = NO;
}

- (CGFloat)heightAboveFeed {
  CGFloat heightAboveFeed = self.view.safeAreaInsets.top;
  for (UIViewController* viewController in self.viewControllersAboveFeed) {
    heightAboveFeed += viewController.view.frame.size.height;
  }
  return heightAboveFeed;
}

- (CGFloat)scrollPosition {
  return self.collectionView.contentOffset.y;
}

- (CGFloat)pinnedOffsetY {
  return [self.headerViewController pinnedOffsetY] - self.additionalOffset;
}

- (void)omniboxDidResignFirstResponder {
  if (![self.headerViewController isShowing] && !self.scrolledToMinimumHeight) {
    return;
  }

  self.omniboxFocused = NO;
  [self shiftTilesDownForOmniboxDefocus];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // If `feedWrapperViewController` is nil, then the NTP is either being created
  // or updated and is not ready to handle scroll events. Doing so could cause
  // unexpected behavior, such as breaking the layout or causing crashes.
  if (!self.feedWrapperViewController) {
    return;
  }
  [self.overscrollActionsController scrollViewDidScroll:scrollView];
  [self.panGestureHandler scrollViewDidScroll:scrollView];
  [self updateFakeOmniboxForScrollPosition];

  [self updateScrolledToMinimumHeight];

  CGFloat scrollPosition = scrollView.contentOffset.y;
  [self handleStickyElementsForScrollPosition:scrollPosition force:NO];

  if (self.viewDidAppear) {
    [self updateFeedSigninPromoIsVisible];
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  // User has interacted with the surface, so it is safe to assume that a saved
  // scroll position can now be overriden.
  self.hasSavedOffsetFromPreviousScrollState = NO;
  [self.overscrollActionsController scrollViewWillBeginDragging:scrollView];
  [self.panGestureHandler scrollViewWillBeginDragging:scrollView];
  self.scrollStartPosition = scrollView.contentOffset.y;
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  [self.overscrollActionsController
      scrollViewWillEndDragging:scrollView
                   withVelocity:velocity
            targetContentOffset:targetContentOffset];
  [self.panGestureHandler scrollViewWillEndDragging:scrollView
                                       withVelocity:velocity
                                targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  [self.overscrollActionsController scrollViewDidEndDragging:scrollView
                                              willDecelerate:decelerate];
  if (self.isFeedVisible) {
    [self.feedMetricsRecorder recordFeedScrolled:scrollView.contentOffset.y -
                                                 self.scrollStartPosition];
  }
}

- (void)scrollViewDidScrollToTop:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewWillBeginDecelerating:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
  [self.panGestureHandler scrollViewDidEndDecelerating:scrollView];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  // TODO(crbug.com/1114792): Handle scrolling.
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  // User has tapped the status bar to scroll to the top.
  // Prevent scrolling back to pre-focus state, making sure we don't have
  // two scrolling animations running at the same time.
  self.collectionShiftingOffset = 0;
  // Reset here since none of the view lifecycle callbacks are called reliably
  // to be able to be used (it seems) (i.e. switching between NTPs where there
  // is saved scroll state in the destination tab). If the content offset is
  // being set to the top, it is safe to assume this can be set to NO. Being
  // called before setSavedContentOffset: is no problem since then it will be
  // subsequently overriden to YES.
  self.hasSavedOffsetFromPreviousScrollState = NO;
  // Unfocus omnibox without scrolling back.
  [self unfocusOmnibox];
  return YES;
}

#pragma mark - NewTabPageOmniboxPositioning

- (CGFloat)stickyOmniboxHeight {
  // Takes the height of the entire header and subtracts the margin to stick the
  // fake omnibox. Adjusts this for the device by further subtracting the
  // toolbar height and safe area insets.
  return [self.headerViewController headerHeight] -
         ntp_header::kFakeOmniboxScrolledToTopMargin -
         ToolbarExpandedHeight(
             [UIApplication sharedApplication].preferredContentSizeCategory) -
         self.view.safeAreaInsets.top - [self feedHeaderHeight];
}

#pragma mark - ThumbStripSupporting

- (BOOL)isThumbStripEnabled {
  return self.panGestureHandler != nil;
}

- (void)thumbStripEnabledWithPanHandler:
    (ViewRevealingVerticalPanHandler*)panHandler {
  DCHECK(!self.thumbStripEnabled);
  self.panGestureHandler = panHandler;
}

- (void)thumbStripDisabled {
  DCHECK(self.thumbStripEnabled);
  self.panGestureHandler = nil;
}

#pragma mark - UIGestureRecognizerDelegate

// TODO(crbug.com/1170995): Remove once the Feed header properly supports
// ContentSuggestions.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Ignore all touches inside the Feed CollectionView, which includes
  // ContentSuggestions.
  UIView* viewToIgnoreTouches = self.collectionView;
  CGRect ignoreBoundsInView =
      [viewToIgnoreTouches convertRect:viewToIgnoreTouches.bounds
                                toView:self.view];
  return !(CGRectContainsPoint(ignoreBoundsInView,
                               [touch locationInView:self.view]));
}

#pragma mark - Scrolling Animations

// Updates the collection view's scroll view offset for the next frame of the
// shiftTilesDownForOmniboxDefocus animation.
- (void)shiftTilesDownAnimationDidFire:(CADisplayLink*)link {
  // If this is the first frame of the animation, store the starting timestamp
  // and do nothing.
  if (self.shiftTileStartTime == -1) {
    self.shiftTileStartTime = link.timestamp;
    return;
  }

  CFTimeInterval timeElapsed = link.timestamp - self.shiftTileStartTime;
  double percentComplete = timeElapsed / kShiftTilesDownAnimationDuration;
  // Ensure that the percentage cannot be above 1.0.
  if (percentComplete > 1.0) {
    percentComplete = 1.0;
  }

  // Find how much the collection view should be scrolled up in the next frame.
  CGFloat yOffset = (1.0 - percentComplete) * [self pinnedOffsetY] +
                    percentComplete * MAX([self pinnedOffsetY] -
                                              self.collectionShiftingOffset,
                                          -self.additionalOffset);
  self.collectionView.contentOffset = CGPointMake(0, yOffset);

  if (percentComplete == 1.0) {
    [link invalidate];
    self.collectionShiftingOffset = 0;
    // Reset `shiftTileStartTime` to its sentinel value.
    self.shiftTileStartTime = -1;
  }
}

- (void)shiftTilesUpToFocusOmnibox {
  // Add gesture recognizer to collection view when the omnibox is focused.
  [self.view addGestureRecognizer:self.tapGestureRecognizer];

  if (self.collectionView.decelerating) {
    // Stop the scrolling if the scroll view is decelerating to prevent the
    // focus to be immediately lost.
    [self.collectionView setContentOffset:self.collectionView.contentOffset
                                 animated:NO];
  }

  if (self.scrolledToMinimumHeight) {
    self.shouldAnimateHeader = NO;
    self.disableScrollAnimation = NO;
    [self.ntpContentDelegate focusOmnibox];
    [self.headerViewController
        completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:
            UIViewAnimatingPositionEnd];
    return;
  }

  if (CGSizeEqualToSize(self.collectionView.contentSize, CGSizeZero)) {
    [self.collectionView layoutIfNeeded];
  }

  CGFloat headerPinnedOffsetY = [self.headerViewController pinnedOffsetY];
  self.collectionShiftingOffset = MAX(
      -self.additionalOffset, headerPinnedOffsetY - [self adjustedOffset].y);
  self.shouldAnimateHeader = YES;

  CGFloat pinnedOffsetBeforeAnimation = [self pinnedOffsetY];
  __weak __typeof(self) weakSelf = self;

  ProceduralBlock shiftOmniboxToTop = ^{
    __typeof(weakSelf) strongSelf = weakSelf;
    // Changing the contentOffset of the collection results in a
    // scroll and a change in the constraints of the header.
    strongSelf.collectionView.contentOffset =
        CGPointMake(0, [strongSelf pinnedOffsetY]);
    // Layout the header for the constraints to be animated.
    [strongSelf.headerViewController layoutHeader];
    //    [strongSelf.collectionView.collectionViewLayout invalidateLayout];
  };

  self.animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:kShiftTilesUpAnimationDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              NewTabPageViewController* strongSelf = weakSelf;
              if (!strongSelf) {
                return;
              }

              if (strongSelf.collectionView.contentOffset.y <
                  [strongSelf pinnedOffsetY]) {
                self.disableScrollAnimation = YES;
                [strongSelf.headerViewController expandHeaderForFocus];
                shiftOmniboxToTop();
                [strongSelf.ntpContentDelegate focusOmnibox];
              }
            }];

  [self.animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    NewTabPageViewController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    if (finalPosition == UIViewAnimatingPositionEnd) {
      // Content suggestion headers can be updated during the scroll, causing
      // `pinnedOffsetY` to be invalid. When this happens during the animation,
      // the tiles are not scrolled to the top causing the omnibox to be hidden
      // by the `PrimaryToolbarView`. In that state, the omnibox's popup and the
      // keyboard are still visible.
      // If the animation is not interrupted and `pinnedOffsetY` changed
      // during the animation, shift the omnibox to the top at the end of the
      // animation.
      if ([strongSelf pinnedOffsetY] != pinnedOffsetBeforeAnimation &&
          strongSelf.collectionView.contentOffset.y <
              [strongSelf pinnedOffsetY]) {
        shiftOmniboxToTop();
      }
      strongSelf.shouldAnimateHeader = NO;
    }

    strongSelf.scrolledToMinimumHeight = YES;
    strongSelf.disableScrollAnimation = NO;
    [strongSelf.headerViewController
        completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:finalPosition];
    strongSelf.isAnimatingOmniboxFocus = NO;
  }];

  self.animator.interruptible = YES;
  self.isAnimatingOmniboxFocus = YES;
  [self.animator startAnimation];
}

#pragma mark - Private

- (UICollectionView*)collectionView {
  return self.feedWrapperViewController.contentCollectionView;
}

// Configures overscroll actions controller.
- (void)configureOverscrollActionsController {
  // Ensure the feed's scroll view exists to prevent crashing the overscroll
  // controller.
  if (!self.collectionView) {
    return;
  }
  // Overscroll action does not work well with content offset, so set this
  // to never and internally offset the UI to account for safe area insets.
  self.collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  self.overscrollActionsController = [[OverscrollActionsController alloc]
      initWithScrollView:self.collectionView];
  [self.overscrollActionsController
      setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
  self.overscrollActionsController.delegate = self.overscrollDelegate;
  [self updateOverscrollActionsState];
}

// Enables or disables overscroll actions.
- (void)updateOverscrollActionsState {
  if (IsSplitToolbarMode(self)) {
    [self.overscrollActionsController enableOverscrollActions];
  } else {
    [self.overscrollActionsController disableOverscrollActions];
  }
}

// Either signals to the omnibox to cancel its focused state or just update the
// NTP state for an unfocused state.
- (void)unfocusOmnibox {
  if (self.omniboxFocused) {
    [self.ntpContentDelegate cancelOmniboxEdit];
  } else {
    [self omniboxDidResignFirstResponder];
  }
}

// Shifts tiles down when defocusing the omnibox.
- (void)shiftTilesDownForOmniboxDefocus {
  if (IsSplitToolbarMode(self)) {
    [self.ntpContentDelegate onFakeboxBlur];
  }

  [self.view removeGestureRecognizer:self.tapGestureRecognizer];

  self.shouldAnimateHeader = YES;

  if (self.animator.running) {
    [self.animator stopAnimation:NO];
    [self.animator finishAnimationAtPosition:UIViewAnimatingPositionStart];
    self.animator = nil;
  }

  if (self.collectionShiftingOffset == 0 || self.collectionView.dragging) {
    self.collectionShiftingOffset = 0;
    [self updateFakeOmniboxForScrollPosition];
    return;
  }

  self.scrolledToMinimumHeight = NO;

  // CADisplayLink is used for this animation instead of the standard UIView
  // animation because the standard animation did not properly convert the
  // fakebox from its scrolled up mode to its scrolled down mode. Specifically,
  // calling `UICollectionView reloadData` adjacent to the standard animation
  // caused the fakebox's views to jump incorrectly. CADisplayLink avoids this
  // problem because it allows `shiftTilesDownAnimationDidFire` to directly
  // control each frame.
  // TODO(crbug.com/1403613): Remove the use of this, listen to the UIScrollView
  // delegate.
  CADisplayLink* link = [CADisplayLink
      displayLinkWithTarget:self
                   selector:@selector(shiftTilesDownAnimationDidFire:)];
  [link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
}

// Pins the fake omnibox to the top of the NTP.
- (void)pinFakeOmniboxToTop {
  self.fakeOmniboxPinnedToTop = YES;
  [self stickFakeOmniboxToTop];
}

// Resets the fake omnibox to its original position.
- (void)resetFakeOmniboxConstraints {
  self.fakeOmniboxPinnedToTop = NO;
  [self setInitialFakeOmniboxConstraints];
}

// Lets this view own the fake omnibox and sticks it to the top of the NTP.
- (void)stickFakeOmniboxToTop {
  // If `self.headerViewController` is nil after removing it from the view
  // hierarchy it means its no longer owned by anyone (e.g. The coordinator
  // might have been stopped.) and we shouldn't try to add it again.
  if (!self.headerViewController) {
    return;
  }

  [NSLayoutConstraint deactivateConstraints:self.fakeOmniboxConstraints];

  self.headerTopAnchor = [self.headerViewController.view.topAnchor
      constraintEqualToAnchor:self.feedWrapperViewController.view.topAnchor
                     constant:-([self stickyOmniboxHeight] +
                                [self feedHeaderHeight])];
  // This issue fundamentally comes down to the topAnchor being set just once
  // and if it is set in landscape mode, it never is updated upon rotation.
  // And landscape is when it doesn't matter.
  self.fakeOmniboxConstraints = @[
    self.headerTopAnchor,
    [self.headerViewController.view.leadingAnchor
        constraintEqualToAnchor:self.feedWrapperViewController.view
                                    .leadingAnchor],
    [self.headerViewController.view.trailingAnchor
        constraintEqualToAnchor:self.feedWrapperViewController.view
                                    .trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:self.fakeOmniboxConstraints];
}

// Gives content suggestions collection view ownership of the fake omnibox for
// the width animation.
- (void)setInitialFakeOmniboxConstraints {
  [NSLayoutConstraint deactivateConstraints:self.fakeOmniboxConstraints];
  self.fakeOmniboxConstraints = @[
    [self.contentSuggestionsViewController.view.topAnchor
        constraintEqualToAnchor:self.headerViewController.view.bottomAnchor],
  ];
  [NSLayoutConstraint activateConstraints:self.fakeOmniboxConstraints];
}

// Update the header for a new width size depending on if the change needs to be
// animated.
- (void)updateFakeOmniboxOnNewWidth:(CGFloat)width {
  if (self.shouldAnimateHeader) {
    // We check -superview here because in certain scenarios (such as when the
    // VC is rotated underneath another presented VC), in a
    // UICollectionViewController -viewSafeAreaInsetsDidChange the VC.view has
    // updated safeAreaInsets, but VC.collectionView does not until a layer
    // -viewDidLayoutSubviews.  Since self.collectionView and it's superview
    // should always have the same safeArea, this should be safe.
    UIEdgeInsets insets = self.collectionView.superview.safeAreaInsets;
    [self.headerViewController
        updateFakeOmniboxForOffset:[self adjustedOffset].y
                       screenWidth:width
                    safeAreaInsets:insets
            animateScrollAnimation:!self.disableScrollAnimation];
  } else {
    [self.headerViewController updateFakeOmniboxForWidth:width];
  }
}

// Update the header state for a change in scroll position. This could mean
// unfocusing the omnibox and/or updating its shape if `shouldAnimateHeader` is
// YES.
- (void)updateFakeOmniboxForScrollPosition {
  // Unfocus the omnibox when the scroll view is scrolled by the user (but not
  // when a scroll is triggered by layout/UIKit).
  if (self.omniboxFocused && !self.shouldAnimateHeader &&
      self.collectionView.dragging) {
    [self unfocusOmnibox];
  }

  if (self.shouldAnimateHeader) {
    UIEdgeInsets insets = self.collectionView.safeAreaInsets;
    [self.headerViewController
        updateFakeOmniboxForOffset:[self adjustedOffset].y
                       screenWidth:self.collectionView.frame.size.width
                    safeAreaInsets:insets
            animateScrollAnimation:!self.disableScrollAnimation];
  }
}

// Pins feed header to top of the NTP when scrolled into the feed, below the
// omnibox.
- (void)stickFeedHeaderToTop {
  DCHECK(self.feedHeaderViewController);
  DCHECK(IsWebChannelsEnabled());

  [NSLayoutConstraint deactivateConstraints:self.feedHeaderConstraints];

  // If the fake omnibox is pinned to the top, we pin the feed header below it.
  // Otherwise, the feed header gets pinned to the top.
  if ([self shouldPinFakeOmnibox]) {
    self.feedHeaderConstraints = @[
      [self.feedHeaderViewController.view.topAnchor
          constraintEqualToAnchor:self.headerViewController.view.bottomAnchor
                         constant:-(content_suggestions::HeaderBottomPadding() +
                                    [self.feedHeaderViewController
                                            customSearchEngineViewHeight])],
      [self.collectionView.topAnchor
          constraintEqualToAnchor:self.contentSuggestionsViewController.view
                                      .bottomAnchor],
    ];
  } else {
    self.feedHeaderConstraints = @[
      [self.feedHeaderViewController.view.topAnchor
          constraintEqualToAnchor:self.view.topAnchor
                         constant:-[self.feedHeaderViewController
                                          customSearchEngineViewHeight]],
      [self.collectionView.topAnchor
          constraintEqualToAnchor:self.contentSuggestionsViewController.view
                                      .bottomAnchor],
    ];
  }

  [self.feedHeaderViewController
      toggleBackgroundBlur:[self.ntpContentDelegate isContentHeaderSticky]
                  animated:YES];
  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Sets initial feed header constraints, between content suggestions and feed.
- (void)setInitialFeedHeaderConstraints {
  DCHECK(self.feedHeaderViewController);
  [NSLayoutConstraint deactivateConstraints:self.feedHeaderConstraints];

  // If Feed top section is enabled, the header bottom anchor should be set to
  // its top anchor instead of the feed collection's top anchor.
  UIView* bottomView = self.collectionView;
  if (IsDiscoverFeedTopSyncPromoEnabled() &&
      self.feedTopSectionViewController) {
    bottomView = self.feedTopSectionViewController.view;
  }
  self.feedHeaderConstraints = @[
    [self.feedHeaderViewController.view.topAnchor
        constraintEqualToAnchor:self.contentSuggestionsViewController.view
                                    .bottomAnchor],
    [bottomView.topAnchor constraintEqualToAnchor:self.feedHeaderViewController
                                                      .view.bottomAnchor],
  ];
  [self.feedHeaderViewController toggleBackgroundBlur:NO animated:YES];
  [NSLayoutConstraint activateConstraints:self.feedHeaderConstraints];
}

// Sets an top inset to the feed collection view to fit the content above it.
- (void)updateFeedInsetsForContentAbove {
  self.collectionView.contentInset = UIEdgeInsetsMake(
      [self heightAboveFeed], 0, self.collectionView.contentInset.bottom, 0);
  // Updates `additionalOffset` using the content above the feed.
  self.additionalOffset = [self heightAboveFeed];
  // Update `scrolledToMinimumHeight` after updating `additionalOffset`.
  [self updateScrolledToMinimumHeight];
}

// Checks whether the feed top section is visible and updates the
// `ntpContentDelegate`.
- (void)updateFeedSigninPromoIsVisible {
  if (!self.feedTopSectionViewController) {
    return;
  }

  // The y-position where NTP content starts being visible.
  CGFloat visibleContentStartingPoint = [self scrollPosition] +
                                        self.view.frame.size.height -
                                        self.view.safeAreaInsets.top;

  // Signin promo is logged as visible when at least the top 2/3 or bottom 1/3
  // of it can be seen. This is not logged if the user focuses the omnibox since
  // the suggestion sheet covers the NTP content.
  BOOL isFeedSigninPromoVisible =
      (visibleContentStartingPoint > -([self feedTopSectionHeight] * 2) / 3 &&
       ([self scrollPosition] <
        -([self stickyContentHeight] + [self feedTopSectionHeight] / 3))) &&
      !self.omniboxFocused;

  [self.ntpContentDelegate
      signinPromoHasChangedVisibility:isFeedSigninPromoVisible];
}

// TODO(crbug.com/1403612): Remove once the Feed header properly supports
// ContentSuggestions.
- (void)handleSingleTapInView:(UITapGestureRecognizer*)recognizer {
  CGPoint location = [recognizer locationInView:[recognizer.view superview]];
  CGRect discBoundsInView =
      [self.identityDiscButton convertRect:self.identityDiscButton.bounds
                                    toView:self.view];
  if (CGRectContainsPoint(discBoundsInView, location)) {
    [self.identityDiscButton
        sendActionsForControlEvents:UIControlEventTouchUpInside];
  } else {
    [self unfocusOmnibox];
  }
}

// Handles the pinning of the sticky elements to the top of the NTP. This
// includes the fake omnibox and if Web Channels is enabled, the feed header. If
// `force` is YES, the sticky elements will always be set based on the scroll
// position. If `force` is NO, the sticky elements will only based on
// `isScrolledIntoFeed` to prevent pinning them multiple times.
// TODO(crbug.com/1277504): Modify this comment when Web Channels is released.
- (void)handleStickyElementsForScrollPosition:(CGFloat)scrollPosition
                                        force:(BOOL)force {
  // Handles the sticky omnibox. Does not stick for iPads.
  if ([self shouldPinFakeOmnibox]) {
    if (scrollPosition > [self offsetToStickOmnibox] &&
        (!self.fakeOmniboxPinnedToTop || force)) {
      [self pinFakeOmniboxToTop];
    } else if (scrollPosition <= [self offsetToStickOmnibox] &&
               (self.fakeOmniboxPinnedToTop || force)) {
      [self resetFakeOmniboxConstraints];
    }
  } else if (self.fakeOmniboxPinnedToTop) {
    [self resetFakeOmniboxConstraints];
  }

  // Handles the sticky feed header.
  if ([self.ntpContentDelegate isContentHeaderSticky] &&
      self.feedHeaderViewController) {
    if ((!self.isScrolledIntoFeed || force) &&
        scrollPosition > [self offsetWhenScrolledIntoFeed]) {
      [self setIsScrolledIntoFeed:YES];
      [self stickFeedHeaderToTop];
    } else if ((self.isScrolledIntoFeed || force) &&
               scrollPosition <= [self offsetWhenScrolledIntoFeed]) {
      [self setIsScrolledIntoFeed:NO];
      [self setInitialFeedHeaderConstraints];
    }
  }

  // Content suggestions header will sometimes glitch when swiping quickly from
  // inside the feed to the top of the NTP. This check safeguards this action to
  // make sure the header is properly positioned. (crbug.com/1261458)
  if ([self isNTPScrolledToTop]) {
    [self setInitialFakeOmniboxConstraints];
    if ([self.ntpContentDelegate isContentHeaderSticky]) {
      [self setInitialFeedHeaderConstraints];
    }
  }
}

// Registers notifications for certain actions on the NTP.
- (void)registerNotifications {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(deviceOrientationDidChange)
                 name:UIDeviceOrientationDidChangeNotification
               object:nil];
}

// Handles device rotation.
- (void)deviceOrientationDidChange {
  if (self.viewDidAppear && self.isFeedVisible) {
    [self.feedMetricsRecorder
        recordDeviceOrientationChanged:[[UIDevice currentDevice] orientation]];
  }
}

// The Discover Feed component seems to add an unwanted width constraint
// (<= 360) in some circumstances, including multiwindow on iPad. This
// cleans up the width constraints so proper constraints can be added.
- (void)cleanUpCollectionViewConstraints {
  auto* collectionWidthAnchor = self.collectionView.widthAnchor;
  auto predicate =
      [NSPredicate predicateWithBlock:^BOOL(NSLayoutConstraint* constraint,
                                            NSDictionary* bindings) {
        return constraint.firstAnchor == collectionWidthAnchor;
      }];
  auto collectionWidthConstraints =
      [self.collectionView.constraints filteredArrayUsingPredicate:predicate];
  [NSLayoutConstraint deactivateConstraints:collectionWidthConstraints];
}

// Applies constraints to the NTP collection view, along with the constraints
// for the content suggestions within it.
- (void)applyCollectionViewConstraints {
  UIView* contentSuggestionsView = self.contentSuggestionsViewController.view;
  contentSuggestionsView.translatesAutoresizingMaskIntoConstraints = NO;

  if (self.feedHeaderViewController) {
    [self cleanUpCollectionViewConstraints];

    NSLayoutConstraint* headerWidthConstraint =
        [self.feedHeaderViewController.view.widthAnchor
            constraintEqualToAnchor:self.collectionView.widthAnchor];
    headerWidthConstraint.priority = UILayoutPriorityDefaultHigh;

    [NSLayoutConstraint activateConstraints:@[
      [self.feedHeaderViewController.view.centerXAnchor
          constraintEqualToAnchor:self.collectionView.centerXAnchor],
      [self.feedHeaderViewController.view.widthAnchor
          constraintLessThanOrEqualToConstant:kDiscoverFeedContentWidth],
      headerWidthConstraint,
      [self.collectionView.centerXAnchor
          constraintEqualToAnchor:[self containerView].centerXAnchor],
      [self.collectionView.widthAnchor
          constraintLessThanOrEqualToConstant:kDiscoverFeedContentWidth],
    ]];
    [self setInitialFeedHeaderConstraints];
    if (IsDiscoverFeedTopSyncPromoEnabled() &&
        self.feedTopSectionViewController) {
      [NSLayoutConstraint activateConstraints:@[
        [self.feedTopSectionViewController.view.leftAnchor
            constraintEqualToAnchor:self.collectionView.leftAnchor],
        [self.feedTopSectionViewController.view.widthAnchor
            constraintEqualToAnchor:self.collectionView.widthAnchor],
        [self.feedTopSectionViewController.view.topAnchor
            constraintEqualToAnchor:self.feedHeaderViewController.view
                                        .bottomAnchor],
        [self.collectionView.topAnchor
            constraintEqualToAnchor:self.feedTopSectionViewController.view
                                        .bottomAnchor],
      ]];
    }
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [self.collectionView.topAnchor
          constraintEqualToAnchor:contentSuggestionsView.bottomAnchor],
    ]];
  }

  [NSLayoutConstraint activateConstraints:@[
    [[self containerView].safeAreaLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.headerViewController.view.leadingAnchor],
    [[self containerView].safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:self.headerViewController.view.trailingAnchor],
  ]];
  [self setInitialFakeOmniboxConstraints];

  [NSLayoutConstraint activateConstraints:@[
    [[self containerView].safeAreaLayoutGuide.leadingAnchor
        constraintEqualToAnchor:contentSuggestionsView.leadingAnchor],
    [[self containerView].safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:contentSuggestionsView.trailingAnchor],
  ]];
}

// Sets minimum height for the NTP collection view, allowing it to scroll enough
// to focus the omnibox.
- (void)setMinimumHeight {
  CGFloat minimumNTPHeight = [self minimumNTPHeight] - [self heightAboveFeed];
  self.collectionView.contentSize =
      CGSizeMake(self.view.frame.size.width, minimumNTPHeight);
}

// Sets the content offset to the top of the feed.
- (void)scrollIntoFeed {
  [self setContentOffset:[self offsetWhenScrolledIntoFeed]];
}

// The total height of all sticky content.
- (CGFloat)stickyContentHeight {
  CGFloat stickyContentHeight = [self stickyOmniboxHeight];
  if ([self.ntpContentDelegate isContentHeaderSticky]) {
    stickyContentHeight += [self feedHeaderHeight];
  }
  return stickyContentHeight;
}

// Returns y-offset compensated for any additionalOffset that might be set.
- (CGPoint)adjustedOffset {
  CGPoint adjustedOffset = self.collectionView.contentOffset;
  adjustedOffset.y += self.additionalOffset;
  return adjustedOffset;
}

#pragma mark - Helpers

- (UIViewController*)contentSuggestionsViewController {
  return _contentSuggestionsViewController;
}

- (CGFloat)minimumNTPHeight {
  CGFloat collectionViewHeight = self.collectionView.bounds.size.height;
  CGFloat headerHeight = [self.headerViewController headerHeight];

  // The minimum height for the collection view content should be the height
  // of the header plus the height of the collection view minus the height of
  // the NTP bottom bar. This allows the Most Visited cells to be scrolled up
  // to the top of the screen. Also computes the total NTP scrolling height
  // for Discover infinite feed.
  CGFloat ntpHeight = collectionViewHeight + headerHeight;
  CGFloat minimumHeight =
      ntpHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsRegularXRegularSizeClass(self.collectionView)) {
    CGFloat toolbarHeight =
        IsSplitToolbarMode(self.collectionView)
            ? ToolbarExpandedHeight([UIApplication sharedApplication]
                                        .preferredContentSizeCategory)
            : 0;
    CGFloat additionalHeight =
        toolbarHeight + self.collectionView.contentInset.bottom;
    minimumHeight -= additionalHeight;
  }

  return minimumHeight;
}

// Returns the current height of the content suggestions content.
- (CGFloat)contentSuggestionsContentHeight {
  return [self.contentSuggestionsViewController contentSuggestionsHeight];
}

// Content suggestions height adjusted with the safe area top insets.
- (CGFloat)adjustedContentSuggestionsHeight {
  return [self contentSuggestionsContentHeight] + self.view.safeAreaInsets.top;
}

// Height of the feed header, returns 0 if it is not visible.
- (CGFloat)feedHeaderHeight {
  return self.feedHeaderViewController
             ? [self.feedHeaderViewController feedHeaderHeight] +
                   [self.feedHeaderViewController customSearchEngineViewHeight]
             : 0;
}

// Height of the feed top section, returns 0 if not visible.
- (CGFloat)feedTopSectionHeight {
  return IsDiscoverFeedTopSyncPromoEnabled() &&
                 self.feedTopSectionViewController
             ? self.feedTopSectionViewController.view.frame.size.height
             : 0;
}

// The y-position content offset for when the user has completely scrolled into
// the Feed. Only takes sticky omnibox into consideration for non-iPad devices.
- (CGFloat)offsetWhenScrolledIntoFeed {
  CGFloat offset;
  if ([self shouldPinFakeOmnibox]) {
    offset = -(self.headerViewController.view.frame.size.height -
               [self stickyOmniboxHeight] -
               [self.feedHeaderViewController customSearchEngineViewHeight] -
               content_suggestions::HeaderBottomPadding());
  } else {
    offset = -[self feedHeaderHeight];
  }

  if (self.feedTopSectionViewController) {
    offset -= self.feedTopSectionViewController.view.frame.size.height;
  }

  return offset;
}

// The y-position content offset for when the fake omnibox
// should stick to the top of the NTP.
- (CGFloat)offsetToStickOmnibox {
  CGFloat offset =
      -(self.headerViewController.view.frame.size.height -
        [self stickyOmniboxHeight] -
        [self.feedHeaderViewController customSearchEngineViewHeight]);
  if (IsSplitToolbarMode(self)) {
    offset -= [self contentSuggestionsContentHeight];
  }
  if (self.feedTopSectionViewController) {
    offset -= self.feedTopSectionViewController.view.frame.size.height;
  }
  return offset;
}

// Whether the collection view has attained its minimum height.
// The fake omnibox never actually disappears; the NTP just scrolls enough so
// that it's hidden behind the real one when it's focused. When the NTP hasn't
// fully loaded yet, there isn't enough height to scroll it behind the real
// omnibox, so they would both show.
- (BOOL)collectionViewHasLoaded {
  return self.collectionView.contentSize.height > 0;
}

// TODO(crbug.com/1262536): Temporary fix to compensate for the view hierarchy
// sometimes breaking. Use DCHECKs to investigate what exactly is broken and
// find a fix.
- (void)verifyNTPViewHierarchy {
  // The view hierarchy with the feed enabled should be: self.view ->
  // self.feedWrapperViewController.view ->
  // self.feedWrapperViewController.feedViewController.view ->
  // self.collectionView -> self.contentSuggestionsViewController.view.
  if (![self.collectionView.subviews
          containsObject:self.contentSuggestionsViewController.view]) {
    // Remove child VC from old parent.
    [self.contentSuggestionsViewController willMoveToParentViewController:nil];
    [self.contentSuggestionsViewController removeFromParentViewController];
    [self.contentSuggestionsViewController.view removeFromSuperview];
    [self.contentSuggestionsViewController didMoveToParentViewController:nil];

    // Add child VC to new parent.
    [self.contentSuggestionsViewController
        willMoveToParentViewController:self.feedWrapperViewController
                                           .feedViewController];
    [self.feedWrapperViewController.feedViewController
        addChildViewController:self.contentSuggestionsViewController];
    [self.collectionView addSubview:self.contentSuggestionsViewController.view];
    [self.contentSuggestionsViewController
        didMoveToParentViewController:self.feedWrapperViewController
                                          .feedViewController];

    [self.feedMetricsRecorder
        recordBrokenNTPHierarchy:BrokenNTPHierarchyRelationship::
                                     kContentSuggestionsParent];
  }

  [self ensureView:self.headerViewController.view
             isSubviewOf:self.collectionView
      withRelationshipID:BrokenNTPHierarchyRelationship::
                             kContentSuggestionsHeaderParent];

  [self ensureView:self.feedHeaderViewController.view
             isSubviewOf:self.collectionView
      withRelationshipID:BrokenNTPHierarchyRelationship::kFeedHeaderParent];
  [self ensureView:self.collectionView
             isSubviewOf:self.feedWrapperViewController.feedViewController.view
      withRelationshipID:BrokenNTPHierarchyRelationship::kELMCollectionParent];
  [self ensureView:self.feedWrapperViewController.feedViewController.view
             isSubviewOf:self.feedWrapperViewController.view
      withRelationshipID:BrokenNTPHierarchyRelationship::kDiscoverFeedParent];
  [self ensureView:self.feedWrapperViewController.view
             isSubviewOf:self.view
      withRelationshipID:BrokenNTPHierarchyRelationship::
                             kDiscoverFeedWrapperParent];
}

// Ensures that `subView` is a descendent of `parentView`. If not, logs a DCHECK
// and adds the subview. Includes `relationshipID` for metrics recorder to log
// which part of the view hierarchy was broken.
// TODO(crbug.com/1262536): Remove this once bug is fixed.
- (void)ensureView:(UIView*)subView
           isSubviewOf:(UIView*)parentView
    withRelationshipID:(BrokenNTPHierarchyRelationship)relationship {
  if (![parentView.subviews containsObject:subView]) {
    DCHECK([parentView.subviews containsObject:subView]);
    [subView removeFromSuperview];
    [parentView addSubview:subView];
    [self.feedMetricsRecorder recordBrokenNTPHierarchy:relationship];
  }
}

// Checks if the collection view is scrolled at least to the minimum height and
// updates property.
- (void)updateScrolledToMinimumHeight {
  CGFloat scrollPosition = [self scrollPosition];
  CGFloat offset = [self pinnedOffsetY];

  self.scrolledToMinimumHeight = scrollPosition >= offset;
}

// Adds `viewController` as a child of `parentViewController` and adds
// `viewController`'s view as a subview of `self.collectionView`.
- (void)addViewControllerAboveFeed:(UIViewController*)viewController {
  // Gets the current parent view controller based on feed visibility.
  UIViewController* parentViewController =
      self.isFeedVisible ? self.feedWrapperViewController.feedViewController
                         : self.feedWrapperViewController;

  // Adds view controller and its view as children of the parent view
  // controller.
  [viewController willMoveToParentViewController:parentViewController];
  [parentViewController addChildViewController:viewController];
  [self.collectionView addSubview:viewController.view];
  [viewController didMoveToParentViewController:parentViewController];

  // Adds view controller to array of view controllers above feed.
  [self.viewControllersAboveFeed addObject:viewController];
}

// Removes `viewController` and its corresponding view from the view hierarchy.
- (void)removeFromViewHierarchy:(UIViewController*)viewController {
  [viewController willMoveToParentViewController:nil];
  [viewController.view removeFromSuperview];
  [viewController removeFromParentViewController];
  [viewController didMoveToParentViewController:nil];
}

// Whether the fake omnibox gets pinned to the top, or becomes the real primary
// toolbar. The former is for narrower devices like portait iPhones, and the
// latter is for wider devices like iPads and landscape iPhones.
- (BOOL)shouldPinFakeOmnibox {
  return !IsRegularXRegularSizeClass(self) && IsSplitToolbarMode(self);
}

#pragma mark - Getters

// Returns the container view of the NTP content, depending on prefs and flags.
- (UIView*)containerView {
  UIView* containerView;
  if (self.isFeedVisible) {
    // TODO(crbug.com/1262536): Remove this when the bug is fixed.
    if (IsNTPViewHierarchyRepairEnabled()) {
      [self verifyNTPViewHierarchy];
    }
    containerView = self.feedWrapperViewController.feedViewController.view;
  } else {
    containerView = self.view;
  }
  return containerView;
}

#pragma mark - Setters

// Sets whether or not the NTP is scrolled into the feed and notifies the
// content suggestions layout to avoid it changing the omnibox frame when this
// view controls its position.
- (void)setIsScrolledIntoFeed:(BOOL)scrolledIntoFeed {
  _scrolledIntoFeed = scrolledIntoFeed;
}

// Sets the y content offset of the NTP collection view.
- (void)setContentOffset:(CGFloat)offset {
  self.collectionView.contentOffset = CGPointMake(0, offset);
  self.scrolledIntoFeed = offset > [self offsetWhenScrolledIntoFeed];
  [self handleStickyElementsForScrollPosition:offset force:YES];
  if (self.feedHeaderViewController) {
    [self.feedHeaderViewController
        toggleBackgroundBlur:(self.scrolledIntoFeed &&
                              [self.ntpContentDelegate isContentHeaderSticky])
                    animated:NO];
  }
}

@end
