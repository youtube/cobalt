// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/instant_signin/instant_signin_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation InstantSigninMediator {
  AuthenticationFlow* _authenticationFlow;
  AccessPoint _accessPoint;
  // Completion block to call once AuthenticationFlow is done while being
  // interrupted.
  ProceduralBlock _interruptionCompletion;
}

- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super init];
  if (self) {
    _accessPoint = accessPoint;
  }
  return self;
}

#pragma mark - Public

- (void)startSignInOnlyFlowWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow {
  CHECK(!_authenticationFlow);
  _authenticationFlow = authenticationFlow;
  signin_metrics::RecordSigninUserActionForAccessPoint(_accessPoint);
  __weak __typeof(self) weakSelf = self;
  [_authenticationFlow
      startSignInWithCompletion:^(SigninCoordinatorResult result) {
        [weakSelf signInFlowCompletedForSignInOnlyWithResult:result];
      }];
}

- (void)disconnect {
  CHECK(!_authenticationFlow);
  CHECK(!_interruptionCompletion);
}

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  CHECK(_authenticationFlow);
  _interruptionCompletion = [completion copy];
  [_authenticationFlow interruptWithAction:action];
}

#pragma mark - Private

// Called when the sign-in flow is over.
- (void)signInFlowCompletedForSignInOnlyWithResult:
    (SigninCoordinatorResult)result {
  CHECK(_authenticationFlow);
  _authenticationFlow = nil;
  ProceduralBlock interruptionCompletion = _interruptionCompletion;
  _interruptionCompletion = nil;
  [self.delegate instantSigninMediator:self didSigninWithResult:result];
  if (interruptionCompletion) {
    interruptionCompletion();
  }
}

@end
