// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_detail_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_action_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_controller.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_delegate.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_instructions_coordinator.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_screenshot_view_controller.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "url/gurl.h"

@interface WhatsNewDetailCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    ConfirmationAlertActionHandler>

// The view controller used to display a feature or chrome tip.
@property(nonatomic, strong)
    WhatsNewDetailViewController* whatsNewDetailViewController;
// The view controller used to display a screenshot of a feature or chrome tip.
@property(nonatomic, strong)
    WhatsNewScreenshotViewController* whatsNewScreenshotViewController;
// The Half screen view controller used to display the instructions for a
// feature or chrome tip.
@property(nonatomic, strong)
    WhatsNewInstructionsCoordinator* whatsNewInstructionsCoordinator;
// What's New Item
@property(nonatomic, strong) WhatsNewItem* item;
// The delegate object that manages interactions with the primary action.
@property(nonatomic, weak) id<WhatsNewDetailViewActionHandler> actionHandler;
// The starting time of the detail view.
@property(nonatomic, assign) base::TimeTicks startTime;

@end

@implementation WhatsNewDetailCoordinator

@synthesize baseNavigationController = _baseNavigationController;
@synthesize browser = _browser;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                            item:(WhatsNewItem*)item
                                   actionHandler:
                                       (id<WhatsNewDetailViewActionHandler>)
                                           actionHandler {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _browser = browser;
    _baseNavigationController = navigationController;
    self.item = item;
    self.actionHandler = actionHandler;

    if (IsWhatsNewM116Enabled()) {
      self.whatsNewScreenshotViewController =
          [[WhatsNewScreenshotViewController alloc] initWithWhatsNewItem:item];
      self.whatsNewScreenshotViewController.actionHandler = self;
      self.whatsNewScreenshotViewController.delegate = self;
    } else {
      self.whatsNewDetailViewController = [[WhatsNewDetailViewController alloc]
              initWithParams:item.bannerImage
                       title:item.title
                    subtitle:item.subtitle
          primaryActionTitle:item.primaryActionTitle
            instructionSteps:item.instructionSteps
                        type:item.type
               primaryAction:item.primaryAction
                learnMoreURL:item.learnMoreURL];
      self.whatsNewDetailViewController.actionHandler = self.actionHandler;
      self.whatsNewDetailViewController.delegate = self;
    }
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (IsWhatsNewM116Enabled()) {
    [self.baseNavigationController
        pushViewController:self.whatsNewScreenshotViewController
                  animated:YES];
    self.startTime = base::TimeTicks::Now();
  } else {
    [self.baseNavigationController
        pushViewController:self.whatsNewDetailViewController
                  animated:YES];
  }

  [super start];
}

- (void)stop {
  if (IsWhatsNewM116Enabled()) {
    if ([self.baseNavigationController.viewControllers
            containsObject:self.whatsNewScreenshotViewController]) {
      [self.baseNavigationController
          popToViewController:self.whatsNewScreenshotViewController
                     animated:NO];
      [self.baseNavigationController popViewControllerAnimated:NO];
      [self logTimeSpentOnDetailView];
    }
  } else {
    if ([self.baseNavigationController.viewControllers
            containsObject:self.whatsNewDetailViewController]) {
      [self.baseNavigationController
          popToViewController:self.whatsNewDetailViewController
                     animated:NO];
      [self.baseNavigationController popViewControllerAnimated:NO];
    }
  }

  self.whatsNewDetailViewController = nil;
  self.whatsNewScreenshotViewController = nil;

  [super stop];
}

#pragma mark - WhatsNewDetailViewDelegate

- (void)dismissWhatsNewDetailView:
    (WhatsNewDetailViewController*)whatsNewDetailViewController {
  DCHECK_EQ(self.whatsNewDetailViewController, whatsNewDetailViewController);
  [self dismiss];
}

- (void)dismissWhatsNewInstructionsCoordinator:
    (WhatsNewInstructionsCoordinator*)coordinator {
  [self dismissOnlyWhatsNewInstructionsCoordinator:coordinator];
  [self dismiss];
}

- (void)dismissOnlyWhatsNewInstructionsCoordinator:
    (WhatsNewInstructionsCoordinator*)coordinator {
  DCHECK_EQ(self.whatsNewInstructionsCoordinator, coordinator);
  [self.whatsNewInstructionsCoordinator stop];
}

- (void)dismissWhatsNewScreenshotViewController:
    (WhatsNewScreenshotViewController*)whatsNewScreenshotViewController {
  DCHECK_EQ(self.whatsNewScreenshotViewController,
            whatsNewScreenshotViewController);
  [self dismiss];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.actionHandler didTapActionButton:self.item.type
                           primaryAction:self.item.primaryAction];
}

- (void)confirmationAlertSecondaryAction {
  [self.actionHandler didTapInstructions:self.item.type];
  self.whatsNewInstructionsCoordinator =
      [[WhatsNewInstructionsCoordinator alloc]
          initWithBaseViewController:self.whatsNewScreenshotViewController
                             browser:self.browser
                                item:self.item
                       actionHandler:self.actionHandler];
  self.whatsNewInstructionsCoordinator.delegate = self;
  [self.whatsNewInstructionsCoordinator start];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismiss];
}

- (void)dismiss {
  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  DCHECK(handler);

  [handler dismissWhatsNew];
}

#pragma mark Private

- (void)logTimeSpentOnDetailView {
  const char* type = WhatsNewTypeToStringM116(self.item.type);
  if (!type) {
    return;
  }

  std::string metric = base::StrCat({"IOS.WhatsNew.", type, ".TimeSpent"});
  UmaHistogramMediumTimes(metric.c_str(),
                          base::TimeTicks::Now() - self.startTime);
}

@end
