// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator+subclassing.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_accessibility_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_presentation_state.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarCoordinator () <InfobarCoordinatorImplementation,
                                  InfobarBannerPositioner,
                                  InfobarModalPositioner> {
  // The AnimatedFullscreenDisable disables fullscreen by displaying the
  // Toolbar/s when an Infobar banner is presented.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> _animatedFullscreenDisabler;
}

// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly) infobars::InfoBarDelegate* infobarDelegate;
// The transition delegate used by the Coordinator to present the InfobarBanner.
// nil if no Banner is being presented.
@property(nonatomic, strong)
    InfobarBannerTransitionDriver* bannerTransitionDriver;
// The transition delegate used by the Coordinator to present the InfobarModal.
// nil if no Modal is being presented.
@property(nonatomic, strong)
    InfobarModalTransitionDriver* modalTransitionDriver;
// Readwrite redefinition.
@property(nonatomic, assign, readwrite) BOOL bannerWasPresented;
// YES if the banner is in the process of being dismissed.
@property(nonatomic, assign) BOOL bannerIsBeingDismissed;

@end

@implementation InfobarCoordinator {
  // Timer used to schedule the auto-dismiss of the banner.
  base::OneShotTimer _autoDismissBannerTimer;
}

// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize baseViewController = _baseViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize browser = _browser;

- (instancetype)initWithInfoBarDelegate:
                    (infobars::InfoBarDelegate*)infoBarDelegate
                           badgeSupport:(BOOL)badgeSupport
                                   type:(InfobarType)infobarType {
  self = [super initWithBaseViewController:nil browser:nil];
  if (self) {
    _infobarDelegate = infoBarDelegate;
    _infobarType = infobarType;
    _shouldUseDefaultDismissal = YES;
  }
  return self;
}

#pragma mark - Public Methods.

- (void)stop {
  // Cancel any scheduled automatic dismissal block.
  _autoDismissBannerTimer.Stop();
  _animatedFullscreenDisabler = nullptr;
  _badgeDelegate = nil;
  _infobarDelegate = nil;
}

- (void)presentInfobarBannerAnimated:(BOOL)animated
                          completion:(ProceduralBlock)completion {
  DCHECK(self.browser);
  DCHECK(self.baseViewController);
  DCHECK(self.bannerViewController);
  DCHECK(self.started);

  // If `self.baseViewController` is not part of the ViewHierarchy the banner
  // shouldn't be presented.
  if (!self.baseViewController.view.window) {
    return;
  }

  // Make sure to display the Toolbar/s before presenting the Banner.
  _animatedFullscreenDisabler =
      std::make_unique<AnimatedScopedFullscreenDisabler>(
          FullscreenController::FromBrowser(self.browser));
  _animatedFullscreenDisabler->StartAnimation();

  [self.bannerViewController
      setModalPresentationStyle:UIModalPresentationCustom];
  self.bannerTransitionDriver = [[InfobarBannerTransitionDriver alloc] init];
  self.bannerTransitionDriver.bannerPositioner = self;
  self.bannerViewController.transitioningDelegate = self.bannerTransitionDriver;
  if ([self.bannerViewController
          conformsToProtocol:@protocol(InfobarBannerInteractable)]) {
    UIViewController<InfobarBannerInteractable>* interactableBanner =
        base::mac::ObjCCastStrict<UIViewController<InfobarBannerInteractable>>(
            self.bannerViewController);
    interactableBanner.interactionDelegate = self.bannerTransitionDriver;
  }

  self.infobarBannerState = InfobarBannerPresentationState::IsAnimating;
  [self.baseViewController
      presentViewController:self.bannerViewController
                   animated:animated
                 completion:^{
                   // Capture self in order to make sure the animation dismisses
                   // correctly in case the Coordinator gets stopped mid
                   // presentation. This will also make sure some cleanup tasks
                   // like configuring accessibility for the presenter VC are
                   // performed successfully.
                   [self configureAccessibilityForBannerInViewController:
                             self.baseViewController
                                                              presenting:YES];
                   self.bannerWasPresented = YES;
                   // Set to NO for each Banner this coordinator might present.
                   self.bannerIsBeingDismissed = NO;
                   self.infobarBannerState =
                       InfobarBannerPresentationState::Presented;
                   [self infobarBannerWasPresented];
                   if (completion)
                     completion();
                 }];

  // Dismisses the presented banner after a certain number of seconds.
  if (!UIAccessibilityIsVoiceOverRunning() && self.shouldUseDefaultDismissal) {
    const base::TimeDelta timeDelta =
        self.highPriorityPresentation
            ? kInfobarBannerLongPresentationDuration
            : kInfobarBannerDefaultPresentationDuration;

    // Calling base::OneShotTimer::Start() will cancel any previously scheduled
    // timer, so there is no need to call base::OneShotTimer::Stop() first.
    __weak InfobarCoordinator* weakSelf = self;
    _autoDismissBannerTimer.Start(FROM_HERE, timeDelta, base::BindOnce(^{
                                    [weakSelf dismissInfobarBannerIfReady];
                                  }));
  }
}

- (void)presentInfobarModal {
  DCHECK(self.started);
  ProceduralBlock modalPresentation = ^{
    DCHECK(self.infobarBannerState !=
           InfobarBannerPresentationState::Presented);
    DCHECK(self.baseViewController);
    self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
        initWithTransitionMode:InfobarModalTransitionBase];
    self.modalTransitionDriver.modalPositioner = self;
    __weak __typeof(self) weakSelf = self;
    [self presentInfobarModalFrom:self.baseViewController
                           driver:self.modalTransitionDriver
                       completion:^{
                         [weakSelf infobarModalPresentedFromBanner:NO];
                       }];
  };

  // Dismiss InfobarBanner first if being presented.
  if (self.baseViewController.presentedViewController &&
      self.baseViewController.presentedViewController ==
          self.bannerViewController) {
    [self dismissInfobarBannerAnimated:NO completion:modalPresentation];
  } else {
    modalPresentation();
  }
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  if (!self.infobarDelegate)
    return;

  [self performInfobarAction];
  // If the Banner Button will present the Modal then the banner shouldn't be
  // dismissed.
  if (![self infobarBannerActionWillPresentModal]) {
    [self dismissInfobarBannerAnimated:YES completion:nil];
  }
}

- (void)presentInfobarModalFromBanner {
  DCHECK(self.bannerViewController);
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  self.modalTransitionDriver.modalPositioner = self;
  __weak __typeof(self) weakSelf = self;
  [self presentInfobarModalFrom:self.bannerViewController
                         driver:self.modalTransitionDriver
                     completion:^{
                       [weakSelf infobarModalPresentedFromBanner:YES];
                     }];
}

- (void)dismissInfobarBannerForUserInteraction:(BOOL)userInitiated {
  [self dismissInfobarBannerAnimated:YES
                       userInitiated:userInitiated
                          completion:nil];
}

- (void)infobarBannerWasDismissed {
  DCHECK(self.infobarBannerState == InfobarBannerPresentationState::Presented);

  self.infobarBannerState = InfobarBannerPresentationState::NotPresented;
  [self configureAccessibilityForBannerInViewController:self.baseViewController
                                             presenting:NO];
  self.bannerTransitionDriver = nil;
  _animatedFullscreenDisabler = nullptr;
  [self infobarWasDismissed];
}

#pragma mark InfobarBannerPositioner

- (CGFloat)bannerYPosition {
  NamedGuide* omniboxGuide =
      [NamedGuide guideWithName:kOmniboxGuide
                           view:self.baseViewController.view];
  UIView* omniboxView = omniboxGuide.owningView;
  CGRect omniboxFrame = [omniboxView convertRect:omniboxGuide.layoutFrame
                                          toView:omniboxView.window];
  return CGRectGetMaxY(omniboxFrame);
}

- (UIView*)bannerView {
  return self.bannerViewController.view;
}

#pragma mark InfobarModalDelegate

- (void)modalInfobarButtonWasAccepted:(id)infobarModal {
  [self performInfobarAction];
  [self dismissInfobarModalAnimated:YES];
}

- (void)dismissInfobarModal:(id)infobarModal {
  base::RecordAction(base::UserMetricsAction(kInfobarModalCancelButtonTapped));
  [self dismissInfobarModalAnimated:YES];
}

- (void)modalInfobarWasDismissed:(id)infobarModal {
  self.modalTransitionDriver = nil;

  // If InfobarBanner is being presented it means that this Modal was presented
  // by an InfobarBanner. If this is the case InfobarBanner will call
  // infobarWasDismissed and clean up once it gets dismissed, this prevents
  // counting the dismissal metrics twice.
  if (self.infobarBannerState != InfobarBannerPresentationState::Presented)
    [self infobarWasDismissed];
}

#pragma mark InfobarModalPositioner

- (CGFloat)modalHeightForWidth:(CGFloat)width {
  return [self infobarModalHeightForWidth:width];
}

#pragma mark InfobarCoordinatorImplementation

- (BOOL)configureModalViewController {
  NOTREACHED() << "Subclass must implement.";
  return NO;
}

- (BOOL)isInfobarAccepted {
  NOTREACHED() << "Subclass must implement.";
  return NO;
}

- (BOOL)infobarBannerActionWillPresentModal {
  NOTREACHED() << "Subclass must implement.";
  return NO;
}

- (void)infobarBannerWasPresented {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  NOTREACHED() << "Subclass must implement.";
}

- (void)dismissBannerIfReady {
  NOTREACHED() << "Subclass must implement.";
}

- (BOOL)infobarActionInProgress {
  NOTREACHED() << "Subclass must implement.";
  return NO;
}

- (void)performInfobarAction {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  NOTREACHED() << "Subclass must implement.";
}

- (void)infobarWasDismissed {
  NOTREACHED() << "Subclass must implement.";
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  NOTREACHED() << "Subclass must implement.";
  return 0;
}

#pragma mark - Private

// Dismisses the Infobar banner if it is ready. i.e. the user is no longer
// interacting with it or the Infobar action is still in progress. The dismissal
// will be animated.
- (void)dismissInfobarBannerIfReady {
  if (!self.modalTransitionDriver) {
    [self dismissBannerIfReady];
  }
}

// `presentingViewController` presents the InfobarModal using `driver`. If
// Modal is presented successfully `completion` will be executed.
- (void)presentInfobarModalFrom:(UIViewController*)presentingViewController
                         driver:(InfobarModalTransitionDriver*)driver
                     completion:(ProceduralBlock)completion {
  // `self.modalViewController` only exists while one its being presented, if
  // this is the case early return since there's one already being presented.
  if (self.modalViewController)
    return;

  BOOL infobarWasConfigured = [self configureModalViewController];
  if (!infobarWasConfigured) {
    if (driver.transitionMode == InfobarModalTransitionBanner) {
      [self dismissInfobarBannerAnimated:NO completion:nil];
    }
    return;
  }

  DCHECK(self.modalViewController);
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = driver;
  navController.modalPresentationStyle = UIModalPresentationCustom;
  [presentingViewController presentViewController:navController
                                         animated:YES
                                       completion:completion];
}

// Configures the Banner Accessibility in order to give VoiceOver users the
// ability to select other elements while the banner is presented. Call this
// method after the Banner has been presented or dismissed. `presenting` is YES
// if banner was presented, NO if dismissed.
- (void)configureAccessibilityForBannerInViewController:
            (UIViewController*)presentingViewController
                                             presenting:(BOOL)presenting {
  if (presenting) {
    UpdateBannerAccessibilityForPresentation(presentingViewController,
                                             self.bannerViewController.view);
  } else {
    UpdateBannerAccessibilityForDismissal(presentingViewController);
  }
}

#pragma mark - Dismissal Helpers

// Helper method for non-user initiated InfobarBanner dismissals.
- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion {
  [self dismissInfobarBannerAnimated:animated
                       userInitiated:NO
                          completion:completion];
}

// Helper for banner dismissals.
- (void)dismissInfobarBannerAnimated:(BOOL)animated
                       userInitiated:(BOOL)userInitiated
                          completion:(void (^)())completion {
  DCHECK(self.baseViewController);
  // Make sure the banner is completely presented before trying to dismiss it.
  [self.bannerTransitionDriver completePresentationTransitionIfRunning];

  // The banner dismiss can be triggered concurrently due to different events
  // like swiping it up, entering the TabSwitcher, presenting another VC or the
  // InfobarDelelgate being destroyed. Trying to dismiss it twice might cause a
  // UIKit crash on iOS12.
  if (!self.bannerIsBeingDismissed &&
      self.bannerViewController.presentingViewController) {
    self.bannerIsBeingDismissed = YES;
    [self infobarBannerWillBeDismissed:userInitiated];
    [self.bannerViewController.presentingViewController
        dismissViewControllerAnimated:animated
                           completion:completion];
  } else if (completion) {
    completion();
  }
}

- (void)dismissInfobarModalAnimated:(BOOL)animated {
  [self dismissInfobarModalAnimated:animated completion:nil];
}

@end

@implementation InfobarCoordinator (Subclassing)

- (void)dismissInfobarModalAnimated:(BOOL)animated
                         completion:(ProceduralBlock)completion {
  DCHECK(self.baseViewController);
  UIViewController* presentedViewController =
      self.baseViewController.presentedViewController;
  if (!presentedViewController) {
    if (completion)
      completion();
    return;
  }
  // If the Modal is being presented by the Banner, call dismiss on it.
  // This way the modal dismissal will animate correctly and the completion
  // block cleans up the banner correctly.
  if (self.baseViewController.presentedViewController ==
      self.bannerViewController) {
    __weak __typeof(self) weakSelf = self;
    [self.bannerViewController
        dismissViewControllerAnimated:animated
                           completion:^{
                             [weakSelf dismissInfobarBannerAnimated:NO
                                                         completion:completion];
                           }];

  } else if (presentedViewController ==
             self.modalViewController.navigationController) {
    [self.baseViewController dismissViewControllerAnimated:animated
                                                completion:^{
                                                  if (completion)
                                                    completion();
                                                }];
  }
}

@end
