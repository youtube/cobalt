// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_manager.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface AddAccountSigninCoordinator () <AddAccountSigninManagerDelegate>

// Coordinator to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// Coordinator that handles the sign-in UI flow.
@property(nonatomic, strong) SigninCoordinator* userSigninCoordinator;
// Manager that handles sign-in add account UI.
@property(nonatomic, strong) AddAccountSigninManager* addAccountSigninManager;
// View where the sign-in button was displayed.
@property(nonatomic, assign) AccessPoint accessPoint;
// Promo button used to trigger the sign-in.
@property(nonatomic, assign) PromoAction promoAction;
// Add account sign-in intent.
@property(nonatomic, assign, readonly) AddAccountSigninIntent signinIntent;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation AddAccountSigninCoordinator

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:(AccessPoint)accessPoint
                               promoAction:(PromoAction)promoAction
                              signinIntent:
                                  (AddAccountSigninIntent)signinIntent {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _signinIntent = signinIntent;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
  }
  return self;
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  if (self.userSigninCoordinator) {
    DCHECK(!self.addAccountSigninManager);
    // When interrupting `self.userSigninCoordinator`,
    // `self.userSigninCoordinator.signinCompletion` is called. This callback
    // is in charge to call `[self runCompletionCallbackWithSigninResult:
    // completionInfo:]`.
    [self.userSigninCoordinator interruptWithAction:action
                                         completion:completion];
    return;
  }

  DCHECK(self.addAccountSigninManager);
  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss:
    case SigninCoordinatorInterruptActionDismissWithoutAnimation:
      [self.addAccountSigninManager interruptAddAccountAnimated:NO
                                                     completion:completion];
      break;
    case SigninCoordinatorInterruptActionDismissWithAnimation:
      [self.addAccountSigninManager interruptAddAccountAnimated:YES
                                                     completion:completion];
      break;
  }
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  self.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  id<SystemIdentityInteractionManager> identityInteractionManager =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->CreateInteractionManager();

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.addAccountSigninManager = [[AddAccountSigninManager alloc]
      initWithBaseViewController:self.baseViewController
      identityInteractionManager:identityInteractionManager
                     prefService:self.browser->GetBrowserState()->GetPrefs()
                 identityManager:identityManager];
  self.addAccountSigninManager.delegate = self;
  [self.addAccountSigninManager showSigninWithIntent:self.signinIntent];
}

- (void)stop {
  [super stop];
  // If one of those 3 DCHECK() fails, -[AddAccountSigninCoordinator
  // runCompletionCallbackWithSigninResult] has not been called.
  DCHECK(!self.addAccountSigninManager);
  DCHECK(!self.alertCoordinator);
  DCHECK(!self.userSigninCoordinator);
}

#pragma mark - AddAccountSigninManagerDelegate

- (void)addAccountSigninManagerFailedWithError:(NSError*)error {
  DCHECK(error);
  __weak AddAccountSigninCoordinator* weakSelf = self;
  ProceduralBlock dismissAction = ^{
    [weakSelf.alertCoordinator stop];
    weakSelf.alertCoordinator = nil;
    [weakSelf addAccountSigninManagerFinishedWithSigninResult:
                  SigninCoordinatorResultCanceledByUser
                                                     identity:nil];
  };

  self.alertCoordinator = ErrorCoordinator(
      error, dismissAction, self.baseViewController, self.browser);
  [self.alertCoordinator start];
}

- (void)addAccountSigninManagerFinishedWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                               identity:(id<SystemIdentity>)
                                                            identity {
  if (!self.addAccountSigninManager) {
    // The AddAccountSigninManager callback might be called after the
    // interrupt method. If this is the case, the AddAccountSigninCoordinator
    // is already stopped. This call can be ignored.
    return;
  }
  // Add account is done, we don't need `self.AddAccountSigninManager`
  // anymore.
  self.addAccountSigninManager = nil;
  if (signinResult == SigninCoordinatorResultInterrupted) {
    // Stop the reauth flow.
    [self addAccountDoneWithSigninResult:signinResult identity:nil];
    return;
  }

  if (signinResult == SigninCoordinatorResultSuccess &&
      !self.accountManagerService->IsValidIdentity(identity)) {
    __weak __typeof(self) weakSelf = self;
    // A dispatch is needed to ensure that the alert is displayed after
    // dismissing the signin view.
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf presentSignInWithRestrictedAccountAlert];
    });
    return;
  }

  [self continueAddAccountFlowWithSigninResult:signinResult identity:identity];
}

#pragma mark - Private

// Continues the sign-in workflow according to the sign-in intent
- (void)continueAddAccountFlowWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                      identity:(id<SystemIdentity>)identity {
  switch (self.signinIntent) {
    case AddAccountSigninIntentReauthPrimaryAccount: {
      [self presentUserConsentWithIdentity:identity];
      break;
    }
    case AddAccountSigninIntentAddSecondaryAccount: {
      [self addAccountDoneWithSigninResult:signinResult identity:identity];
      break;
    }
  }
}

// Presents an alert when sign in with a restricted account and then continue
// the sign-in workflow.
- (void)presentSignInWithRestrictedAccountAlert {
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_SIGN_IN_INVALID_ACCOUNT_TITLE)
                         message:l10n_util::GetNSString(
                                     IDS_IOS_SIGN_IN_INVALID_ACCOUNT_MESSAGE)];

  __weak __typeof(self) weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                action:^{
                  [weakSelf.alertCoordinator stop];
                  weakSelf.alertCoordinator = nil;
                  [weakSelf continueAddAccountFlowWithSigninResult:
                                SigninCoordinatorResultCanceledByUser
                                                          identity:nil];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

// Runs callback completion on finishing the add account flow.
- (void)addAccountDoneWithSigninResult:(SigninCoordinatorResult)signinResult
                              identity:(id<SystemIdentity>)identity {
  DCHECK(!self.alertCoordinator);
  DCHECK(!self.userSigninCoordinator);
  // `identity` is set, only and only if the sign-in is successful.
  DCHECK(((signinResult == SigninCoordinatorResultSuccess) && identity) ||
         ((signinResult != SigninCoordinatorResultSuccess) && !identity));
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:identity];
  [self runCompletionCallbackWithSigninResult:signinResult
                               completionInfo:completionInfo];
}

// Presents the user consent screen with `identity` pre-selected.
- (void)presentUserConsentWithIdentity:(id<SystemIdentity>)identity {
  // The UserSigninViewController is presented on top of the currently displayed
  // view controller.
  self.userSigninCoordinator = [SigninCoordinator
      userSigninCoordinatorWithBaseViewController:self.baseViewController
                                          browser:self.browser
                                         identity:identity
                                      accessPoint:self.accessPoint
                                      promoAction:self.promoAction];

  __weak AddAccountSigninCoordinator* weakSelf = self;
  self.userSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf.userSigninCoordinator stop];
        weakSelf.userSigninCoordinator = nil;
        [weakSelf addAccountDoneWithSigninResult:signinResult
                                        identity:signinCompletionInfo.identity];
      };
  [self.userSigninCoordinator start];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, signinIntent: %lu, accessPoint: %d, "
                       @"userSigninCoordinator: %p, addAccountSigninManager: "
                       @"%p, alertCoordinator: %p>",
                       self.class.description, self, self.signinIntent,
                       self.accessPoint, self.userSigninCoordinator,
                       self.addAccountSigninManager, self.alertCoordinator];
}

@end
