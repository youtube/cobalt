// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/forced_signin/forced_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_coordinator.h"
#import "ios/chrome/browser/ui/screen/screen_provider.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ForcedSigninCoordinator () <FirstRunScreenDelegate>

@property(nonatomic, strong) ScreenProvider* screenProvider;
@property(nonatomic, strong) InterruptibleChromeCoordinator* childCoordinator;

// The view controller used by ForcedSigninCoordinator.
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation ForcedSigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            screenProvider:(ScreenProvider*)screenProvider {
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _screenProvider = screenProvider;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  self.navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  self.navigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;

  [self presentScreen:[self.screenProvider nextScreenType]];

  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  DCHECK(!self.navigationController);
  DCHECK(!self.childCoordinator);
  DCHECK(!self.screenProvider);
  [super stop];
}

#pragma mark - Private

// Dismiss the main navigation view controller with an animation and run the
// sign-in completion callback on completion of the animation to finish
// presenting the screens.
- (void)finishPresentingScreens {
  __weak __typeof(self) weakSelf = self;
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  void (^completion)(void) = ^{
    SigninCoordinatorResult result =
        identity ? SigninCoordinatorResultSuccess
                 : SigninCoordinatorResultCanceledByUser;
    [weakSelf finishWithResult:result identity:identity];
  };
  [self.navigationController dismissViewControllerAnimated:YES
                                                completion:completion];
}

// Presents the screen of certain `type`.
- (void)presentScreen:(ScreenType)type {
  // If there are no screens remaining, call delegate to stop presenting
  // screens.
  if (type == kStepsCompleted) {
    [self finishPresentingScreens];
    return;
  }
  self.childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [self.childCoordinator start];
}

// Creates a screen coordinator according to `type`.
- (InterruptibleChromeCoordinator*)createChildCoordinatorWithScreenType:
    (ScreenType)type {
  switch (type) {
    case kSignIn:
      return [[SigninScreenCoordinator alloc]
          initWithBaseNavigationController:self.navigationController
                                   browser:self.browser
                            showFREConsent:NO
                                  delegate:self];
    case kTangibleSync:
    case kDefaultBrowserPromo:
    case kStepsCompleted:
      NOTREACHED() << "Type of screen not supported." << static_cast<int>(type);
      break;
  }
  return nil;
}

- (void)finishWithResult:(SigninCoordinatorResult)result
                identity:(id<SystemIdentity>)identity {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
  self.navigationController = nil;
  self.screenProvider = nil;
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:identity];
  [self runCompletionCallbackWithSigninResult:result
                               completionInfo:completionInfo];
}

#pragma mark - FirstRunScreenDelegate

// This is called before finishing the presentation of a screen.
// Stops the child coordinator and prepares the next screen to present.
- (void)screenWillFinishPresenting {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
  [self presentScreen:[self.screenProvider nextScreenType]];
}

- (void)skipAllScreens {
  [self finishPresentingScreens];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock finishCompletion = ^() {
    [weakSelf finishWithResult:SigninCoordinatorResultInterrupted identity:nil];
    if (completion) {
      completion();
    }
  };
  BOOL animated = NO;
  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss: {
      [self.childCoordinator
          interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
                   completion:^{
                     finishCompletion();
                   }];
      return;
    }
    case SigninCoordinatorInterruptActionDismissWithoutAnimation: {
      animated = NO;
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithAnimation: {
      animated = YES;
      break;
    }
  }

  // Interrupt the child coordinator UI first before dismissing the forced
  // sign-in navigation controller.
  [self.childCoordinator
      interruptWithAction:
          SigninCoordinatorInterruptActionDismissWithoutAnimation
               completion:^{
                 [weakSelf.navigationController.presentingViewController
                     dismissViewControllerAnimated:animated
                                        completion:finishCompletion];
               }];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, screenProvider: %p, childCoordinator: %p, "
                       @"navigationController %p>",
                       self.class.description, self, self.screenProvider,
                       self.childCoordinator, self.navigationController];
}

@end
