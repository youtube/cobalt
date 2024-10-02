// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_presentation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OverlayPresentationController

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:
               (nullable UIViewController*)presentingViewController {
  self = [super initWithPresentedViewController:presentedViewController
                       presentingViewController:presentingViewController];
  if (self) {
    _needsLayout = YES;
  }
  return self;
}

#pragma mark - Accessors

- (BOOL)resizesPresentationContainer {
  return NO;
}

#pragma mark - UIPresentationController

- (BOOL)shouldPresentInFullscreen {
  return NO;
}

- (void)containerViewWillLayoutSubviews {
  [super containerViewWillLayoutSubviews];
  // Trigger a layout pass for the presenting view controller.  This allows the
  // presentation context to resize itself to match the presented overlay UI if
  // `resizesPresentationContainer` is YES.
  if (self.needsLayout) {
    [self.presentingViewController.view setNeedsLayout];
    self.needsLayout = NO;
  }
}

@end
