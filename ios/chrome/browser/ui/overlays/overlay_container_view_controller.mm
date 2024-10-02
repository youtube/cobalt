// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OverlayContainerView : UIView
// The owning view controller.
@property(nonatomic, weak) OverlayContainerViewController* viewController;
@end

@implementation OverlayContainerView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  // Allow touches to go to subviews, but ignore touches that are routed to the
  // container view itself.
  UIView* hitView = [super hitTest:point withEvent:event];
  return hitView == self ? nil : hitView;
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // Only register touches that land inside a subview.  Otherwise, return NO to
  // allow touches to continue to the underlying UI.
  for (UIView* subview in self.subviews) {
    CGPoint adjustedPoint = [subview convertPoint:point fromView:self];
    if ([subview pointInside:adjustedPoint withEvent:event])
      return YES;
  }
  return NO;
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self.viewController.delegate containerViewController:self.viewController
                                        didMoveToWindow:self.window];
}

@end

@implementation OverlayContainerViewController

- (void)loadView {
  OverlayContainerView* containerView = [[OverlayContainerView alloc] init];
  containerView.viewController = self;
  self.view = containerView;
}

@end
