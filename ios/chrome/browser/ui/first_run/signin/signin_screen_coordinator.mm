// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_coordinator.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tos_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_view_controller.h"
#import "ios/chrome/browser/ui/first_run/tos/tos_coordinator.h"
#import "ios/chrome/browser/ui/first_run/uma/uma_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninScreenCoordinator () <IdentityChooserCoordinatorDelegate,
                                       SigninScreenViewControllerDelegate,
                                       TOSCommands,
                                       UMACoordinatorDelegate>

// Show FRE consent.
@property(nonatomic, assign) BOOL showFREConsent;
// First run screen delegate.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;
// Sign-in screen view controller.
@property(nonatomic, strong) SigninScreenViewController* viewController;
// Sign-in screen mediator.
@property(nonatomic, strong) SigninScreenMediator* mediator;
// Account manager service.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// Authentication service.
@property(nonatomic, assign) AuthenticationService* authenticationService;
// Coordinator used to manage the TOS page.
@property(nonatomic, strong) TOSCoordinator* TOSCoordinator;
// Coordinator to show the metric reportingn dialog.
@property(nonatomic, strong) UMACoordinator* UMACoordinator;
// Coordinator to choose an identity.
@property(nonatomic, strong)
    IdentityChooserCoordinator* identityChooserCoordinator;
// Coordinator to add an identity.
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;
@property(nonatomic, assign) BOOL UMAReportingUserChoice;

@end

@implementation SigninScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                  showFREConsent:(BOOL)showFREConsent
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
    _baseNavigationController = navigationController;
    _showFREConsent = showFREConsent;
    _delegate = delegate;
    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
  }
  return self;
}

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TOSCommands)];
  id<TOSCommands> TOSHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TOSCommands);
  self.viewController = [[SigninScreenViewController alloc] init];
  self.viewController.TOSHandler = TOSHandler;
  self.viewController.delegate = self;
  self.viewController.modalInPresentation = YES;

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  self.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  PrefService* localPrefService = GetApplicationContext()->GetLocalState();
  PrefService* prefService = browserState->GetPrefs();
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  self.mediator = [[SigninScreenMediator alloc]
      initWithAccountManagerService:self.accountManagerService
              authenticationService:self.authenticationService
                    identityManager:identityManager
                   localPrefService:localPrefService
                        prefService:prefService
                        syncService:syncService
                     showFREConsent:self.showFREConsent];
  self.mediator.consumer = self.viewController;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
}

- (void)stop {
  [self.identityChooserCoordinator stop];
  self.delegate = nil;
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.accountManagerService = nil;
  self.authenticationService = nil;
}

#pragma mark - Private

// Starts the coordinator to present the Add Account module.
- (void)triggerAddAccount {
  [self.mediator userAttemptedToSignin];
  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_START_PAGE];
  __weak __typeof(self) weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf addAccountSigninCompleteWithResult:signinResult
                                      completionInfo:signinCompletionInfo];
      };
  [self.addAccountSigninCoordinator start];
}

// Callback handling the completion of the AddAccount action.
- (void)addAccountSigninCompleteWithResult:(SigninCoordinatorResult)signinResult
                            completionInfo:
                                (SigninCompletionInfo*)signinCompletionInfo {
  [self.addAccountSigninCoordinator stop];
  self.addAccountSigninCoordinator = nil;
  if (signinResult == SigninCoordinatorResultSuccess &&
      self.accountManagerService->IsValidIdentity(
          signinCompletionInfo.identity)) {
    self.mediator.selectedIdentity = signinCompletionInfo.identity;
    self.mediator.addedAccount = YES;
  }
}

// Starts the sign in process.
- (void)startSignIn {
  DCHECK(self.mediator.selectedIdentity);
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.mediator.selectedIdentity
                                 postSignInAction:PostSignInAction::kNone
                         presentingViewController:self.viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^() {
    [weakSelf finishPresentingWithSignIn:YES];
  };
  [self.mediator startSignInWithAuthenticationFlow:authenticationFlow
                                        completion:completion];
}

// Calls the mediator and the delegate when the coordinator is finished.
- (void)finishPresentingWithSignIn:(BOOL)signIn {
  [self.mediator finishPresentingWithSignIn:signIn];
  [self.delegate screenWillFinishPresenting];
}

// Shows the UMA dialog so the user can manage metric reporting.
- (void)showUMADialog {
  DCHECK(!self.UMACoordinator);
  self.UMACoordinator = [[UMACoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
               UMAReportingValue:self.mediator.UMAReportingUserChoice];
  self.UMACoordinator.delegate = self;
  [self.UMACoordinator start];
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  DCHECK(!self.addAccountSigninCoordinator);
  [self triggerAddAccount];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(id<SystemIdentity>)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.mediator.selectedIdentity = identity;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  switch (self.authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      if (self.mediator.selectedIdentity) {
        [self startSignIn];
      } else {
        [self triggerAddAccount];
      }
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      [self finishPresentingWithSignIn:NO];
      return;
  }
}

- (void)didTapSecondaryActionButton {
  __weak __typeof(self) weakSelf = self;
  [self.mediator cancelSignInScreenWithCompletion:^{
    [weakSelf finishPresentingWithSignIn:NO];
  }];
}

- (void)didTapURLInDisclaimer:(NSURL*)URL {
  if ([URL.absoluteString isEqualToString:first_run::kTermsOfServiceURL]) {
    [self showTOSPage];
  } else if ([URL.absoluteString
                 isEqualToString:first_run::kMetricReportingURL]) {
    self.mediator.UMALinkWasTapped = YES;
    [self showUMADialog];
  } else {
    NOTREACHED() << std::string("Unknown URL ")
                 << base::SysNSStringToUTF8(URL.absoluteString);
  }
}

#pragma mark - SigninScreenViewControllerDelegate

- (void)showAccountPickerFromPoint:(CGPoint)point {
  DCHECK(!self.identityChooserCoordinator);
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity =
      self.mediator.selectedIdentity;
}

#pragma mark - TOSCommands

- (void)showTOSPage {
  DCHECK(!self.TOSCoordinator);
  self.mediator.TOSLinkWasTapped = YES;
  self.TOSCoordinator =
      [[TOSCoordinator alloc] initWithBaseViewController:self.viewController
                                                 browser:self.browser];
  [self.TOSCoordinator start];
}

- (void)closeTOSPage {
  DCHECK(self.TOSCoordinator);
  [self.TOSCoordinator stop];
  self.TOSCoordinator = nil;
}

#pragma mark - UMACoordinatorDelegate

- (void)UMACoordinatorDidRemoveWithCoordinator:(UMACoordinator*)coordinator
                        UMAReportingUserChoice:(BOOL)UMAReportingUserChoice {
  DCHECK(self.UMACoordinator);
  DCHECK_EQ(self.UMACoordinator, coordinator);
  self.UMACoordinator = nil;
  DCHECK(self.mediator);
  self.mediator.UMAReportingUserChoice = UMAReportingUserChoice;
}

@end
