// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator.h"

#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_instructions_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_view_controller.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_mediator.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface SetUpListDefaultBrowserPromoCoordinator () <
    ConfirmationAlertActionHandler>
@end

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;

@implementation SetUpListDefaultBrowserPromoCoordinator {
  // The Default Browser view controller.
  UIViewController* _viewController;

  // Application is used to open the OS settings for this app.
  UIApplication* _application;

  // Whether or not the Set Up List Item should be marked complete.
  BOOL _markItemComplete;

  raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      _deviceSwitcherResultDispatcher;
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      _segmentationService;
  SetUpListDefaultBrowserPromoMediator* _mediator;

  // TODO: (crbug.com/357867254) Transparent view to block user interaction
  // while waiting for classification results. This ivar is a temporary
  // solution.
  UIView* _transparentView;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               application:(UIApplication*)application
                       segmentationService:
                           (segmentation_platform::SegmentationPlatformService*)
                               segmentationService
            deviceSwitcherResultDispatcher:
                (segmentation_platform::DeviceSwitcherResultDispatcher*)
                    dispatcher {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _application = application;
    _segmentationService = segmentationService;
    _deviceSwitcherResultDispatcher = dispatcher;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  RecordAction(UserMetricsAction("IOS.DefaultBrowserPromo.SetUpList.Appear"));
  [self recordDefaultBrowserPromoShown];

  if (IsSegmentedDefaultBrowserPromoEnabled()) {
    CHECK(_segmentationService);
    CHECK(_deviceSwitcherResultDispatcher);
    _mediator = [[SetUpListDefaultBrowserPromoMediator alloc]
           initWithSegmentationService:_segmentationService
        deviceSwitcherResultDispatcher:_deviceSwitcherResultDispatcher];

    // Present a transparent view to block UI interaction until promo presents.
    _transparentView =
        [[UIView alloc] initWithFrame:self.baseViewController.view.bounds];
    _transparentView.backgroundColor = [UIColor clearColor];
    [self.baseViewController.view addSubview:_transparentView];

    __weak __typeof(self) weakSelf = self;
    [_mediator retrieveUserSegmentWithCompletion:^{
      [weakSelf showPromo];
    }];
  } else {
    [self showPromo];
  }
}

- (void)stop {
  _viewController.presentationController.delegate = nil;

  ProceduralBlock completion = nil;
  if (_markItemComplete) {
    PrefService* localState = GetApplicationContext()->GetLocalState();
    completion = ^{
      set_up_list_prefs::MarkItemComplete(localState,
                                          SetUpListItemType::kDefaultBrowser);
    };
  }

  [_viewController dismissViewControllerAnimated:YES completion:completion];

  _viewController = nil;
  _application = nil;
  _transparentView = nil;
  _segmentationService = nullptr;
  _deviceSwitcherResultDispatcher = nullptr;
  if (![self shouldAnimate]) {
    _mediator.consumer = nil;
  }
  [_mediator disconnect];
  _mediator = nil;
  self.delegate = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  base::UmaHistogramEnumeration(
      kSetUpListDefaultBrowserPromoAction,
      SegmentedDefaultBrowserPromoAction::kAnimatedPromoAccept);
  [_mediator didTapPrimaryActionButton];
  _markItemComplete = YES;
  [self.delegate setUpListDefaultBrowserPromoDidFinish:YES];
}

- (void)confirmationAlertSecondaryAction {
  base::UmaHistogramEnumeration(
      kSetUpListDefaultBrowserPromoAction,
      SegmentedDefaultBrowserPromoAction::kAnimatedPromoDismiss);
  _markItemComplete = YES;
  [self.delegate setUpListDefaultBrowserPromoDidFinish:NO];
}

- (void)confirmationAlertTertiaryAction {
  base::UmaHistogramEnumeration(
      kSetUpListDefaultBrowserPromoAction,
      SegmentedDefaultBrowserPromoAction::kAnimatedPromoDismiss);
  [self.delegate setUpListDefaultBrowserPromoDidFinish:NO];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  base::UmaHistogramEnumeration(
      kSetUpListDefaultBrowserPromoAction,
      SegmentedDefaultBrowserPromoAction::kStaticPromoAccept);
  RecordDefaultBrowserPromoLastAction(
      IOSDefaultBrowserPromoAction::kActionButton);
  RecordAction(UserMetricsAction("IOS.DefaultBrowserPromo.SetUpList.Accepted"));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserPromoAction::kActionButton];
  [_application openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:nil];
  _markItemComplete = YES;
  [self.delegate setUpListDefaultBrowserPromoDidFinish:YES];
}

- (void)didTapSecondaryActionButton {
  base::UmaHistogramEnumeration(
      kSetUpListDefaultBrowserPromoAction,
      SegmentedDefaultBrowserPromoAction::kStaticPromoDismiss);
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kCancel);
  RecordAction(UserMetricsAction("IOS.DefaultBrowserPromo.SetUpList.Dismiss"));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserPromoAction::kCancel];
  _markItemComplete = YES;
  [self.delegate setUpListDefaultBrowserPromoDidFinish:NO];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction::kDismiss);
  RecordAction(UserMetricsAction("IOS.DefaultBrowserPromo.SetUpList.Dismiss"));
  [self logDefaultBrowserFullscreenPromoHistogramForAction:
            IOSDefaultBrowserPromoAction::kCancel];
  [self.delegate setUpListDefaultBrowserPromoDidFinish:NO];
}

#pragma mark - Metrics Helpers

- (void)logDefaultBrowserFullscreenPromoHistogramForAction:
    (IOSDefaultBrowserPromoAction)action {
  UmaHistogramEnumeration("IOS.DefaultBrowserPromo.SetUpList.Action", action);
}

#pragma mark - Private

// Records that a default browser promo has been shown.
- (void)recordDefaultBrowserPromoShown {
  ProfileIOS* profile = self.browser->GetProfile();
  LogToFETDefaultBrowserPromoShown(
      feature_engagement::TrackerFactory::GetForProfile(profile));
}

// Presents the default browser promo.
- (void)showPromo {
  [_transparentView removeFromSuperview];
  _transparentView = nil;

  if ([self shouldAnimate]) {
    base::UmaHistogramEnumeration(
        kSetUpListDefaultBrowserPromoAction,
        SegmentedDefaultBrowserPromoAction::kAnimatedPromoAppear);
    DefaultBrowserInstructionsViewController* animatedViewController = [self
        createAnimatedViewControllerWithTitle:[_mediator retrievePromoTitle]];

    CHECK(animatedViewController);
    CHECK(!_transparentView);

    _viewController = animatedViewController;
  } else {
    base::UmaHistogramEnumeration(
        kSetUpListDefaultBrowserPromoAction,
        SegmentedDefaultBrowserPromoAction::kStaticPromoAppear);
    DefaultBrowserScreenViewController* staticViewController =
        [[DefaultBrowserScreenViewController alloc] init];
    staticViewController.delegate = self;

    if (IsSegmentedDefaultBrowserPromoEnabled()) {
      _mediator.consumer = staticViewController;
    }

    _viewController = staticViewController;
  }
  [_viewController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  _viewController.presentationController.delegate = self;
}

// Create the animated View Controller.
- (DefaultBrowserInstructionsViewController*)
    createAnimatedViewControllerWithTitle:(NSString*)promoTitle {
  return [[DefaultBrowserInstructionsViewController alloc]
      initWithDismissButton:YES
           hasRemindMeLater:NO
                   hasSteps:NO
              actionHandler:self
                  titleText:promoTitle];
}

// Determine which version of the Default Browser Promo should be shown.
// Returns `YES` if the animated Default Browser Promo should be shown.
- (BOOL)shouldAnimate {
  return (IsSegmentedDefaultBrowserPromoEnabled() &&
          (SegmentedDefaultBrowserExperimentTypeEnabled() ==
           SegmentedDefaultBrowserExperimentType::kAnimatedPromo));
}

@end
