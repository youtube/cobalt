// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/signin/system_identity_interaction_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AddAccountSigninManager ()

// Presenting view controller.
@property(nonatomic, strong) UIViewController* baseViewController;
// The coordinator's manager that handles interactions to add identities.
@property(nonatomic, strong) id<SystemIdentityInteractionManager>
    identityInteractionManager;
// The Browser state's user-selected preferences.
@property(nonatomic, assign) PrefService* prefService;
// The Browser state's identity manager.
@property(nonatomic, assign) signin::IdentityManager* identityManager;
// Indicates that the add account sign-in flow was interrupted.
@property(nonatomic, readwrite) BOOL signinInterrupted;

@end

@implementation AddAccountSigninManager {
  // YES if the add account if done, and the delegate has been called.
  BOOL _addAccountFlowDone;
}

#pragma mark - Public

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
    identityInteractionManager:
        (id<SystemIdentityInteractionManager>)identityInteractionManager
                   prefService:(PrefService*)prefService
               identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _identityInteractionManager = identityInteractionManager;
    _prefService = prefService;
    _identityManager = identityManager;
  }
  return self;
}

- (void)showSigninWithIntent:(AddAccountSigninIntent)signinIntent {
  DCHECK(!_addAccountFlowDone);
  DCHECK(self.identityInteractionManager);
  NSString* userEmail;
  switch (signinIntent) {
    case AddAccountSigninIntentAddSecondaryAccount: {
      userEmail = nil;
      break;
    }
    case AddAccountSigninIntentReauthPrimaryAccount: {
      CoreAccountInfo accountInfo = self.identityManager->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync);
      std::string userEmailString = accountInfo.email;

      if (userEmailString.empty()) {
        // This corresponds to a re-authenticate request after the user was
        // signed out. This corresponds to the case where the identity was
        // removed as a result of the permissions being removed on the server or
        // the identity being removed from another app.
        //
        // Simply use the the last signed-in user email in this case and go
        // though the entire sign-in flow as sync needs to be configured.
        userEmailString =
            self.prefService->GetString(prefs::kGoogleServicesLastUsername);
      }
      DCHECK(!userEmailString.empty());
      userEmail = base::SysUTF8ToNSString(userEmailString);
      break;
    }
  }
  __weak AddAccountSigninManager* weakSelf = self;
  [self.identityInteractionManager
      startAuthActivityWithViewController:self.baseViewController
                                userEmail:userEmail
                               completion:^(id<SystemIdentity> identity,
                                            NSError* error) {
                                 [weakSelf
                                     operationCompletedWithIdentity:identity
                                                              error:error];
                               }];
}

- (void)interruptAddAccountAnimated:(BOOL)animated
                         completion:(ProceduralBlock)completion {
  self.signinInterrupted = YES;
  __weak __typeof(self) weakSelf = self;
  [self.identityInteractionManager
      cancelAuthActivityAnimated:animated
                      completion:^() {
                        // If `identityInteractionManager` completion callback
                        // has not been called yet, the add account needs to be
                        // fully done by calling:
                        // `operationCompletedWithIdentity:error:`, before
                        // calling `completion` See crbug.com/1227658.
                        [weakSelf operationCompletedWithIdentity:nil error:nil];
                        if (completion) {
                          completion();
                        }
                      }];
}

#pragma mark - Private

// Handles the reauthentication or add account operation or displays an alert
// if the flow is interrupted by a sign-in error.
- (void)operationCompletedWithIdentity:(id<SystemIdentity>)identity
                                 error:(NSError*)error {
  if (_addAccountFlowDone) {
    // When the dialog is interrupted, this method can be called twice.
    // See: `interruptAddAccountAnimated:completion:`.
    return;
  }
  DCHECK(self.identityInteractionManager);
  _addAccountFlowDone = YES;
  self.identityInteractionManager = nil;
  SigninCoordinatorResult signinResult = SigninCoordinatorResultSuccess;
  if (self.signinInterrupted) {
    signinResult = SigninCoordinatorResultInterrupted;
    identity = nil;
  } else if (error) {
    // Filter out errors handled internally by `identity`.
    if (ShouldHandleSigninError(error)) {
      [self.delegate addAccountSigninManagerFailedWithError:error];
      return;
    }
    signinResult = SigninCoordinatorResultCanceledByUser;
    identity = nil;
  }

  [self.delegate addAccountSigninManagerFinishedWithSigninResult:signinResult
                                                        identity:identity];
}

@end
