// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_handler.h"

#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_animation.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_animation_layout_providing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kBrowserToGridDuration = 0.3;
const CGFloat kGridToBrowserDuration = 0.5;
const CGFloat kReducedMotionDuration = 0.25;
}  // namespace

@interface TabGridTransitionHandler ()
@property(nonatomic, weak) id<GridTransitionAnimationLayoutProviding>
    layoutProvider;
// Animation object for the transition.
@property(nonatomic, strong) GridTransitionAnimation* animation;
@end

@implementation TabGridTransitionHandler

#pragma mark - Public

- (instancetype)initWithLayoutProvider:
    (id<GridTransitionAnimationLayoutProviding>)layoutProvider {
  self = [super init];
  if (self) {
    _layoutProvider = layoutProvider;
  }
  return self;
}

- (void)transitionFromBrowser:(UIViewController*)browser
                    toTabGrid:(UIViewController*)tabGrid
                   activePage:(TabGridPage)activePage
               withCompletion:(void (^)(void))completion {
  [browser willMoveToParentViewController:nil];

  if (UIAccessibilityIsReduceMotionEnabled()) {
    [self transitionWithReducedAnimationsForTab:browser.view
                                 beingPresented:NO
                                 withCompletion:^{
                                   [browser.view removeFromSuperview];
                                   [browser removeFromParentViewController];
                                   [tabGrid setNeedsStatusBarAppearanceUpdate];
                                   if (completion)
                                     completion();
                                 }];
    return;
  }

  GridAnimationDirection direction = GridAnimationDirectionContracting;
  CGFloat duration = self.animationDisabled ? 0 : kBrowserToGridDuration;

  self.animation = [[GridTransitionAnimation alloc]
      initWithLayout:[self transitionLayoutForTabInViewController:browser
                                                       activePage:activePage]
            duration:duration
           direction:direction];

  UIView* animationContainer = [self.layoutProvider animationViewsContainer];
  UIView* bottomViewForAnimations =
      [self.layoutProvider animationViewsContainerBottomView];
  [animationContainer insertSubview:self.animation
                       aboveSubview:bottomViewForAnimations];

  UIView* activeItem = self.animation.activeItem;
  UIView* selectedItem = self.animation.selectionItem;
  BOOL shouldReparentSelectedCell =
      [self.layoutProvider shouldReparentSelectedCell:direction];

  if (shouldReparentSelectedCell) {
    [tabGrid.view addSubview:selectedItem];
    [tabGrid.view addSubview:activeItem];
  }

  [self.animation.animator addAnimations:^{
    [tabGrid setNeedsStatusBarAppearanceUpdate];
  }];

  [self.animation.animator addCompletion:^(UIViewAnimatingPosition position) {
    if (shouldReparentSelectedCell) {
      [activeItem removeFromSuperview];
      [selectedItem removeFromSuperview];
    }
    [self.animation removeFromSuperview];
    if (position == UIViewAnimatingPositionEnd) {
      [browser.view removeFromSuperview];
      [browser removeFromParentViewController];
    }
    if (completion)
      completion();
  }];

  // TODO(crbug.com/850507): Have the tab view animate itself out alongside
  // this transition instead of just zeroing the alpha here.
  browser.view.alpha = 0;

  // Run the main animation.
  [self.animation.animator startAnimation];
}

- (void)transitionFromTabGrid:(UIViewController*)tabGrid
                    toBrowser:(UIViewController*)browser
                   activePage:(TabGridPage)activePage
               withCompletion:(void (^)(void))completion {
  [tabGrid addChildViewController:browser];

  browser.view.frame = tabGrid.view.bounds;
  [tabGrid.view addSubview:browser.view];

  browser.view.accessibilityViewIsModal = YES;

  if (self.animationDisabled) {
    browser.view.alpha = 1;
    [tabGrid setNeedsStatusBarAppearanceUpdate];
    if (completion)
      completion();
    return;
  }

  browser.view.alpha = 0;

  if (UIAccessibilityIsReduceMotionEnabled() ||
      !self.layoutProvider.selectedCellVisible) {
    [self transitionWithReducedAnimationsForTab:browser.view
                                 beingPresented:YES
                                 withCompletion:^{
                                   [browser
                                       didMoveToParentViewController:tabGrid];
                                   [tabGrid setNeedsStatusBarAppearanceUpdate];
                                   if (completion)
                                     completion();
                                 }];
    return;
  }

  GridAnimationDirection direction = GridAnimationDirectionExpanding;
  CGFloat duration = self.animationDisabled ? 0 : kGridToBrowserDuration;

  self.animation = [[GridTransitionAnimation alloc]
      initWithLayout:[self transitionLayoutForTabInViewController:browser
                                                       activePage:activePage]
            duration:duration
           direction:direction];

  UIView* animationContainer = [self.layoutProvider animationViewsContainer];
  UIView* bottomViewForAnimations =
      [self.layoutProvider animationViewsContainerBottomView];
  [animationContainer insertSubview:self.animation
                       aboveSubview:bottomViewForAnimations];

  UIView* activeItem = self.animation.activeItem;
  UIView* selectedItem = self.animation.selectionItem;
  BOOL shouldReparentSelectedCell =
      [self.layoutProvider shouldReparentSelectedCell:direction];

  if (shouldReparentSelectedCell) {
    [tabGrid.view addSubview:selectedItem];
    [tabGrid.view addSubview:activeItem];
  }

  [self.animation.animator addAnimations:^{
    [tabGrid setNeedsStatusBarAppearanceUpdate];
  }];

  [self.animation.animator addCompletion:^(UIViewAnimatingPosition position) {
    if (shouldReparentSelectedCell) {
      [activeItem removeFromSuperview];
      [selectedItem removeFromSuperview];
    }
    [self.animation removeFromSuperview];
    if (position == UIViewAnimatingPositionEnd) {
      browser.view.alpha = 1;
      [browser didMoveToParentViewController:tabGrid];
    }
    if (completion)
      completion();
  }];

  // Run the main animation.
  [self.animation.animator startAnimation];
}

#pragma mark - Private

// Returns the transition layout for the `activePage`, based on the `browser`.
- (GridTransitionLayout*)transitionLayoutForTabInViewController:
                             (UIViewController*)viewControllerForTab
                                                     activePage:(TabGridPage)
                                                                    activePage {
  GridTransitionLayout* layout =
      [self.layoutProvider transitionLayout:activePage];

  // Get the fram for the snapshotted content of the active tab.
  // Conceptually the transition is dismissing/presenting a tab (a BVC).
  // However, currently the BVC instances are themselves contanted within a
  // BVCContainer view controller. This means that the
  // `viewControllerForTab.view` is not the BVC's view; rather it's the view of
  // the view controller that contains the BVC. Unfortunatley, the layout guide
  // needed here is attached to the BVC's view, which is the first (and only)
  // subview of the BVCContainerViewController's view.
  // TODO(crbug.com/860234) Clean up this arrangement.
  UIView* tabContentView = viewControllerForTab.view.subviews[0];

  CGRect contentArea = [NamedGuide guideWithName:kContentAreaGuide
                                            view:tabContentView]
                           .layoutFrame;

  [layout.activeItem populateWithSnapshotsFromView:tabContentView
                                        middleRect:contentArea];
  layout.expandedRect = [[self.layoutProvider animationViewsContainer]
      convertRect:tabContentView.frame
         fromView:viewControllerForTab.view];

  return layout;
}

// Animates the transition for the `tab`, whether it is `beingPresented` or not,
// with reduced animations.
- (void)transitionWithReducedAnimationsForTab:(UIView*)tab
                               beingPresented:(BOOL)beingPresented
                               withCompletion:(void (^)(void))completion {
  // The animation here creates a simple quick zoom effect -- the tab view
  // fades in/out as it expands/contracts. The zoom is not large (75% to 100%)
  // and is centered on the view's final center position, so it's not directly
  // connected to any tab grid positions.
  CGFloat tabFinalAlpha;
  CGAffineTransform tabFinalTransform;
  CGFloat tabFinalCornerRadius;

  if (beingPresented) {
    // If presenting, the tab view animates in from 0% opacity, 75% scale
    // transform, and a 26pt corner radius
    tabFinalAlpha = 1;
    tabFinalTransform = tab.transform;
    tab.transform = CGAffineTransformScale(tabFinalTransform, 0.75, 0.75);
    tabFinalCornerRadius = DeviceCornerRadius();
    tab.layer.cornerRadius = 26.0;
  } else {
    // If dismissing, the the tab view animates out to 0% opacity, 75% scale,
    // and 26px corner radius.
    tabFinalAlpha = 0;
    tabFinalTransform = CGAffineTransformScale(tab.transform, 0.75, 0.75);
    tab.layer.cornerRadius = DeviceCornerRadius();
    tabFinalCornerRadius = 26.0;
  }

  // Set clipsToBounds on the animating view so its corner radius will look
  // right.
  BOOL oldClipsToBounds = tab.clipsToBounds;
  tab.clipsToBounds = YES;

  CGFloat duration = self.animationDisabled ? 0 : kReducedMotionDuration;
  [UIView animateWithDuration:duration
      delay:0.0
      options:UIViewAnimationOptionCurveEaseOut
      animations:^{
        tab.alpha = tabFinalAlpha;
        tab.transform = tabFinalTransform;
        tab.layer.cornerRadius = tabFinalCornerRadius;
      }
      completion:^(BOOL finished) {
        // When presenting the FirstRun ViewController, this can be called with
        // `finished` to NO on official builds. For now, the animation not
        // finishing isn't handled anywhere.
        tab.clipsToBounds = oldClipsToBounds;
        if (completion)
          completion();
      }];
}

@end
