// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_container_view_controller.h"

#import "base/check_op.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation GridContainerViewController {
  NSArray<NSLayoutConstraint*>* _containedViewControllerConstraints;
}

- (void)setContainedViewController:(UIViewController*)viewController {
  if (_containedViewController) {
    if (_containedViewControllerConstraints) {
      [self deactivateConstraints];
      _containedViewControllerConstraints = nil;
    }
    [_containedViewController willMoveToParentViewController:nil];
    [_containedViewController.view removeFromSuperview];
    [_containedViewController removeFromParentViewController];
  }
  if (viewController) {
    [self addChildViewController:viewController];
    viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:viewController.view];
    [self setConstraintsRelativeToView:viewController.view];
    [viewController didMoveToParentViewController:self];
  }
  _containedViewController = viewController;
}

- (void)activateConstraints {
  if (_containedViewControllerConstraints) {
    [NSLayoutConstraint
        activateConstraints:_containedViewControllerConstraints];
  }

  _containedViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
}

- (void)deactivateConstraints {
  if (_containedViewControllerConstraints) {
    [NSLayoutConstraint
        deactivateConstraints:_containedViewControllerConstraints];
  }

  _containedViewController.view.translatesAutoresizingMaskIntoConstraints = YES;
}

#pragma mark - Private

// Positions `view` to be aligned with the GridContainer's view on all sides.
- (void)setConstraintsRelativeToView:(UIView*)view {
  _containedViewControllerConstraints = @[
    [view.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [view.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [view.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [view.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ];

  [self activateConstraints];
}

@end
