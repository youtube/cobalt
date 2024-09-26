// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/uma/uma_coordinator.h"

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/first_run/uma/uma_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UMACoordinator () <UIAdaptivePresentationControllerDelegate,
                              UMATableViewControllerPresentationDelegate>

@property(nonatomic, strong) UMATableViewController* viewController;
// UMA reporting value when the dialog is opened.
@property(nonatomic, assign) BOOL initialeUMAReportingValue;

@end

@implementation UMACoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                         UMAReportingValue:(BOOL)UMAReportingValue {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _initialeUMAReportingValue = UMAReportingValue;
  }
  return self;
}

- (void)start {
  [super start];
  // Creates the view controller.
  self.viewController =
      [[UMATableViewController alloc] initWithStyle:ChromeTableViewStyle()];
  self.viewController.presentationDelegate = self;
  self.viewController.UMAReportingUserChoice = self.initialeUMAReportingValue;
  // Creates the navigation controller and present.
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.presentationController.delegate = self;
  if (@available(iOS 15, *)) {
    UISheetPresentationController* presentationController =
        navigationController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
  }
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate
      UMACoordinatorDidRemoveWithCoordinator:self
                      UMAReportingUserChoice:self.viewController
                                                 .UMAReportingUserChoice];
}

#pragma mark - UMATableViewControllerPresentationDelegate

- (void)UMATableViewControllerDidDismiss:
    (UMATableViewController*)viewController {
  DCHECK_EQ(self.viewController, viewController);
  [self.delegate
      UMACoordinatorDidRemoveWithCoordinator:self
                      UMAReportingUserChoice:self.viewController
                                                 .UMAReportingUserChoice];
}

@end
