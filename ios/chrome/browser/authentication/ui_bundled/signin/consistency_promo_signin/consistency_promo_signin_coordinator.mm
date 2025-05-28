// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_layout_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_navigation_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_presentation_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_slide_transition_animator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/interruptible_chrome_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface ConsistencyPromoSigninCoordinator () <
    ConsistencyAccountChooserCoordinatorDelegate,
    ConsistencyDefaultAccountCoordinatorDelegate,
    ConsistencyPromoSigninMediatorDelegate,
    ConsistencyLayoutDelegate,
    UINavigationControllerDelegate,
    UIViewControllerTransitioningDelegate>

// Navigation controller for the consistency promo.
@property(nonatomic, strong)
    ConsistencySheetNavigationController* navigationController;
// Coordinator for the first screen.
@property(nonatomic, strong)
    ConsistencyDefaultAccountCoordinator* defaultAccountCoordinator;
// Coordinator to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// Coordinator to select another identity.
@property(nonatomic, strong)
    ConsistencyAccountChooserCoordinator* accountChooserCoordinator;
// `self.defaultAccountCoordinator.selectedIdentity`.
@property(nonatomic, strong, readonly) id<SystemIdentity> selectedIdentity;
// Coordinator to add an account to the device.
@property(nonatomic, strong) SigninCoordinator* addAccountCoordinator;

@property(nonatomic, strong)
    ConsistencyPromoSigninMediator* consistencyPromoSigninMediator;

@end

@implementation ConsistencyPromoSigninCoordinator

#pragma mark - Public

+ (instancetype)
    coordinatorWithBaseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser
                          accessPoint:(signin_metrics::AccessPoint)accessPoint {
  ProfileIOS* profile = browser->GetProfile();
  if (accessPoint == signin_metrics::AccessPoint::kWebSignin) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(profile);
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForProfile(profile);
    bool hasIdentities = [signin::GetIdentitiesOnDevice(
                             identityManager, accountManagerService) count] > 0;
    if (!hasIdentities) {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::SUPPRESSED_NO_ACCOUNTS,
          accessPoint);
      return nil;
    }
  }
  return [[ConsistencyPromoSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  [self stopAlertCoordinator];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock consistencyCompletion = ^() {
    [weakSelf finalizeInterruptWithAction:action completion:completion];
  };
  if (self.addAccountCoordinator) {
    [self.addAccountCoordinator interruptWithAction:action
                                         completion:consistencyCompletion];
  } else {
    consistencyCompletion();
  }
}

- (void)start {
  [super start];
  signin_metrics::LogSignInStarted(self.accessPoint);
  base::RecordAction(base::UserMetricsAction("Signin_BottomSheet_Opened"));
  // Create ConsistencyPromoSigninMediator.
  ProfileIOS* profile = self.browser->GetProfile();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  self.consistencyPromoSigninMediator = [[ConsistencyPromoSigninMediator alloc]
      initWithAccountManagerService:accountManagerService
              authenticationService:authenticationService
                    identityManager:identityManager
                    userPrefService:profile->GetPrefs()
                        accessPoint:self.accessPoint];
  self.consistencyPromoSigninMediator.delegate = self;
  // Create ConsistencySheetNavigationController so it can be giving as the base
  // view controller for the account coordinator.
  self.navigationController =
      [[ConsistencySheetNavigationController alloc] initWithNibName:nil
                                                             bundle:nil];
  self.navigationController.delegate = self;
  self.navigationController.modalPresentationStyle = UIModalPresentationCustom;
  self.navigationController.transitioningDelegate = self;
  // Create ConsistencyDefaultAccountCoordinator.
  self.defaultAccountCoordinator = [[ConsistencyDefaultAccountCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                     accessPoint:self.accessPoint];
  self.defaultAccountCoordinator.delegate = self;
  self.defaultAccountCoordinator.layoutDelegate = self;
  [self.defaultAccountCoordinator start];
  self.navigationController.viewControllers =
      @[ self.defaultAccountCoordinator.viewController ];
  // Present the view.
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [self stopDefaultAccountCoordinator];
}

#pragma mark - Properties

- (id<SystemIdentity>)selectedIdentity {
  return self.defaultAccountCoordinator.selectedIdentity;
}

#pragma mark - Private

- (void)stopAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

- (void)stopAccountChooserCoordinator {
  self.accountChooserCoordinator.delegate = nil;
  self.accountChooserCoordinator.layoutDelegate = nil;
  [self.accountChooserCoordinator stop];
  self.accountChooserCoordinator = nil;
}

- (void)stopDefaultAccountCoordinator {
  self.defaultAccountCoordinator.delegate = nil;
  self.defaultAccountCoordinator.layoutDelegate = nil;
  [self.defaultAccountCoordinator stop];
  self.defaultAccountCoordinator = nil;
}

- (void)stopAddAccountCoordinator {
  [self.addAccountCoordinator stop];
  self.addAccountCoordinator = nil;
}

// Finishes the interrupt process. This method needs to be called once all
// other dialogs on top of ConsistencyPromoSigninCoordinator are properly
// dismissed.
- (void)finalizeInterruptWithAction:(SigninCoordinatorInterrupt)action
                         completion:(ProceduralBlock)interruptCompletion {
  DCHECK(!self.alertCoordinator);
  DCHECK(!self.addAccountCoordinator);
  __weak ConsistencyPromoSigninCoordinator* weakSelf = self;
  const SigninCoordinatorResult signinResult =
      SigninCoordinatorResultInterrupted;
  ProceduralBlock finishCompletionBlock = ^() {
    [weakSelf runCompletionWithSigninResult:signinResult
                         completionIdentity:nil];
    if (interruptCompletion) {
      interruptCompletion();
    }
  };
  switch (action) {
    case SigninCoordinatorInterrupt::UIShutdownNoDismiss:
      CHECK(!IsInterruptibleCoordinatorAlwaysDismissedEnabled(),
            base::NotFatalUntil::M136);
      // The coordinator is done, no event should happen from the mediator.
      [self coordinatorDoneWithResult:signinResult];
      finishCompletionBlock();
      break;
    case SigninCoordinatorInterrupt::DismissWithoutAnimation:
    case SigninCoordinatorInterrupt::DismissWithAnimation: {
      BOOL animated =
          action == SigninCoordinatorInterrupt::DismissWithAnimation;
      if (IsInterruptibleCoordinatorStoppedSynchronouslyEnabled()) {
        [self.navigationController.presentingViewController
            dismissViewControllerAnimated:animated
                               completion:nil];
        // The coordinator is done, no event should happen from the mediator.
        [self coordinatorDoneWithResult:signinResult];
        finishCompletionBlock();
      } else {
          [self.navigationController.presentingViewController
              dismissViewControllerAnimated:animated
                                 completion:finishCompletionBlock];
          // The coordinator is done, no event should happen from the mediator.
          [self coordinatorDoneWithResult:signinResult];
      }
    }
  }
}

// Does cleanup (metrics and remove coordinator) once the add-account flow is
// finished. If `hasAccounts == NO` and `signinResult` is successful , the
// function immediately signs in to Chrome with the identity acquired from the
// add-account flow after the cleanup.
- (void)
    addAccountCompletionWithSigninResult:(SigninCoordinatorResult)signinResult
                      completionIdentity:(id<SystemIdentity>)completionIdentity
                             hasAccounts:(BOOL)hasAccounts {
  if (hasAccounts) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::ADD_ACCOUNT_COMPLETED,
        self.accessPoint);
  } else {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::
            ADD_ACCOUNT_COMPLETED_WITH_NO_DEVICE_ACCOUNT,
        self.accessPoint);
  }

  [self stopAddAccountCoordinator];

  if (signinResult != SigninCoordinatorResultSuccess) {
    return;
  }
  DCHECK(completionIdentity);
  [self.consistencyPromoSigninMediator systemIdentityAdded:completionIdentity];
  self.defaultAccountCoordinator.selectedIdentity = completionIdentity;

  if (hasAccounts) {
    return;
  }
  [self startSignIn];
}

// Opens an AddAccountSigninCoordinator to add an account to the device.
// If `hasAccounts == NO`, the added account will be used to sign in to Chrome
// directly after the AddAccountSigninCoordinator finishes.
- (void)openAddAccountCoordinatorWithHasAccounts:(BOOL)hasAccounts {
  if (hasAccounts) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::ADD_ACCOUNT_STARTED,
        self.accessPoint);
  } else {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::
            ADD_ACCOUNT_STARTED_WITH_NO_DEVICE_ACCOUNT,
        self.accessPoint);
  }
  DCHECK(!self.addAccountCoordinator);
  self.addAccountCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.navigationController
                                          browser:self.browser
                                      accessPoint:self.accessPoint];
  __weak ConsistencyPromoSigninCoordinator* weakSelf = self;
  self.addAccountCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        id<SystemIdentity> signinCompletionIdentity) {
        [weakSelf addAccountCompletionWithSigninResult:signinResult
                                    completionIdentity:signinCompletionIdentity
                                           hasAccounts:hasAccounts];
      };
  [self.addAccountCoordinator start];
}

// Stops all the coordinators and mediator.
- (void)coordinatorDoneWithResult:(SigninCoordinatorResult)signinResult {
  switch (signinResult) {
    case SigninCoordinatorResultCanceledByUser:
      base::RecordAction(
          base::UserMetricsAction("Signin_BottomSheet_ClosedByCancel"));
      break;
    case SigninCoordinatorResultSuccess:
      base::RecordAction(
          base::UserMetricsAction("Signin_BottomSheet_ClosedBySignIn"));
      break;
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted:
      base::RecordAction(
          base::UserMetricsAction("Signin_BottomSheet_ClosedByInterrupt"));
      break;
    case SigninCoordinatorUINotAvailable:
      // ConsistencyPromoSigninCoordinator presents its child coordinators
      // directly and does not use `ShowSigninCommand`.
      NOTREACHED();
  }
  DCHECK(!self.alertCoordinator);
  self.navigationController.delegate = nil;
  self.navigationController.transitioningDelegate = nil;
  self.navigationController = nil;
  [self stopDefaultAccountCoordinator];
  [self stopAccountChooserCoordinator];
  self.consistencyPromoSigninMediator.delegate = nil;
  [self.consistencyPromoSigninMediator disconnectWithResult:signinResult];
  self.consistencyPromoSigninMediator = nil;
}

// Starts the sign-in flow.
- (void)startSignIn {
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.selectedIdentity
                                      accessPoint:self.accessPoint
                                postSignInActions:PostSignInActionSet()
                         presentingViewController:self.navigationController];
  authenticationFlow.precedingHistorySync = YES;
  [self.consistencyPromoSigninMediator
      signinWithAuthenticationFlow:authenticationFlow];
}

#pragma mark - ConsistencyAccountChooserCoordinatorDelegate

- (void)consistencyAccountChooserCoordinatorIdentitySelected:
    (ConsistencyAccountChooserCoordinator*)coordinator {
  self.defaultAccountCoordinator.selectedIdentity =
      self.accountChooserCoordinator.selectedIdentity;
  [self stopAccountChooserCoordinator];
  [self.navigationController popViewControllerAnimated:YES];
}

- (void)consistencyAccountChooserCoordinatorOpenAddAccount:
    (ConsistencyAccountChooserCoordinator*)coordinator {
  [self openAddAccountCoordinatorWithHasAccounts:YES];
}

#pragma mark - ConsistencyDefaultAccountCoordinatorDelegate

- (void)consistencyDefaultAccountCoordinatorSkip:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  // This DCHECK is to help to understand crbug.com/372272374.
  DCHECK(!self.alertCoordinator) << base::SysNSStringToUTF8([self description]);
  ProfileIOS* profile = self.browser->GetProfile();
  PrefService* userPrefService = profile->GetPrefs();
  if (self.accessPoint == signin_metrics::AccessPoint::kWebSignin) {
    const int skipCounter =
        userPrefService->GetInteger(prefs::kSigninWebSignDismissalCount) + 1;
    userPrefService->SetInteger(prefs::kSigninWebSignDismissalCount,
                                skipCounter);
  }
  __weak __typeof(self) weakSelf = self;
  const SigninCoordinatorResult signinResult =
      SigninCoordinatorResultCanceledByUser;
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^() {
                           [weakSelf runCompletionWithSigninResult:signinResult
                                                completionIdentity:nil];
                         }];
  // The coordinator is done, no event should happen from the mediator.
  [self coordinatorDoneWithResult:signinResult];
}

- (void)consistencyDefaultAccountCoordinatorOpenIdentityChooser:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  self.accountChooserCoordinator = [[ConsistencyAccountChooserCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser];
  self.accountChooserCoordinator.delegate = self;
  self.accountChooserCoordinator.layoutDelegate = self;
  [self.accountChooserCoordinator
      startWithSelectedIdentity:self.defaultAccountCoordinator
                                    .selectedIdentity];
  [self.navigationController
      pushViewController:self.accountChooserCoordinator.viewController
                animated:YES];
}

- (void)consistencyDefaultAccountCoordinatorSignin:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  DCHECK_EQ(coordinator, self.defaultAccountCoordinator);
  [self startSignIn];
}

- (void)consistencyDefaultAccountCoordinatorOpenAddAccount:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  [self openAddAccountCoordinatorWithHasAccounts:NO];
}

#pragma mark - ConsistencyLayoutDelegate

- (ConsistencySheetDisplayStyle)displayStyle {
  return self.navigationController.displayStyle;
}

#pragma mark - UINavigationControllerDelegate

- (id<UIViewControllerAnimatedTransitioning>)
               navigationController:
                   (UINavigationController*)navigationController
    animationControllerForOperation:(UINavigationControllerOperation)operation
                 fromViewController:(UIViewController*)fromVC
                   toViewController:(UIViewController*)toVC {
  DCHECK_EQ(navigationController, self.navigationController);
  switch (operation) {
    case UINavigationControllerOperationNone:
      return nil;
    case UINavigationControllerOperationPush:
      return [[ConsistencySheetSlideTransitionAnimator alloc]
             initWithAnimation:ConsistencySheetSlideAnimationPushing
          navigationController:self.navigationController];
    case UINavigationControllerOperationPop:
      return [[ConsistencySheetSlideTransitionAnimator alloc]
             initWithAnimation:ConsistencySheetSlideAnimationPopping
          navigationController:self.navigationController];
  }
  NOTREACHED();
}

- (id<UIViewControllerInteractiveTransitioning>)
                           navigationController:
                               (UINavigationController*)navigationController
    interactionControllerForAnimationController:
        (id<UIViewControllerAnimatedTransitioning>)animationController {
  return self.navigationController.interactionTransition;
}

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  DCHECK(navigationController == self.navigationController);
  DCHECK(navigationController.viewControllers.count > 0);
  DCHECK(navigationController.viewControllers[0] ==
         self.defaultAccountCoordinator.viewController);
  if (self.navigationController.viewControllers.count == 1 &&
      self.accountChooserCoordinator) {
    // AccountChooserCoordinator has been removed by "Back" button.
    [self stopAccountChooserCoordinator];
  }
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presentedViewController
                            presentingViewController:
                                (UIViewController*)presentingViewController
                                sourceViewController:(UIViewController*)source {
  DCHECK_EQ(self.navigationController, presentedViewController);
  return [[ConsistencySheetPresentationController alloc]
      initWithConsistencySheetNavigationController:self.navigationController
                          presentingViewController:presentingViewController];
}

#pragma mark - ConsistencyPromoSigninMediatorDelegate

- (void)consistencyPromoSigninMediatorSigninStarted:
    (ConsistencyPromoSigninMediator*)mediator {
  [self.defaultAccountCoordinator startSigninSpinner];
}

- (void)consistencyPromoSigninMediatorSignInDone:
            (ConsistencyPromoSigninMediator*)mediator
                                    withIdentity:(id<SystemIdentity>)identity {
  DCHECK([identity isEqual:self.selectedIdentity]);
  id<SystemIdentity> completionIdentity = identity;
  __weak __typeof(self) weakSelf = self;
  const SigninCoordinatorResult signinResult = SigninCoordinatorResultSuccess;
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^() {
                           [weakSelf.defaultAccountCoordinator
                                   stopSigninSpinner];
                           [weakSelf runCompletionWithSigninResult:signinResult
                                                completionIdentity:
                                                    completionIdentity];
                         }];
  // The coordinator is done, no event should happen from the mediator.
  [self coordinatorDoneWithResult:SigninCoordinatorResultSuccess];
}

- (void)consistencyPromoSigninMediatorSignInCancelled:
    (ConsistencyPromoSigninMediator*)mediator {
  [self.defaultAccountCoordinator stopSigninSpinner];
}

- (void)consistencyPromoSigninMediator:(ConsistencyPromoSigninMediator*)mediator
                        errorDidHappen:
                            (ConsistencyPromoSigninMediatorError)error {
  NSString* errorTitle = l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_TITLE);
  NSString* errorMessage = nil;
  switch (error) {
    case ConsistencyPromoSigninMediatorErrorGeneric:
      errorMessage =
          l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_GENERIC_ERROR);
      break;
    case ConsistencyPromoSigninMediatorErrorTimeout:
      errorMessage =
          l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_TIMEOUT_ERROR);
      break;
  }
  DCHECK(!self.alertCoordinator);
  [self.defaultAccountCoordinator stopSigninSpinner];
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                           title:errorTitle
                         message:errorMessage];

  __weak __typeof(self) weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SIGN_IN_DISMISS)
                action:^() {
                  [weakSelf stopAlertCoordinator];
                }
                 style:UIAlertActionStyleCancel];
  [self.alertCoordinator start];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, defaultAccountCoordinator: %p, alertCoordinator: %p, "
          @"accountChooserCoordinator %p, addAccountCoordinator %p, presented: "
          @"%@, base viewcontroller: %@ %@>",
          self.class.description, self, self.defaultAccountCoordinator,
          self.alertCoordinator, self.accountChooserCoordinator,
          self.addAccountCoordinator,
          ViewControllerPresentationStatusDescription(
              self.navigationController),
          NSStringFromClass(self.baseViewController.class),
          ViewControllerPresentationStatusDescription(self.baseViewController)];
}

@end
