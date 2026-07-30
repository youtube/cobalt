// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "ios/web/public/web_state.h"

@implementation ActorOverlayCoordinator {
  // The `WebState` of the active tab undergoing actuation.
  base::WeakPtr<web::WebState> _webState;
}

#pragma mark - ActorOverlayCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(webState, base::NotFatalUntil::M165);
    if (!webState) {
      return nil;
    }
    _webState = webState->GetWeakPtr();
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  // TODO(crbug.com/507509954): Implement blue shimmer UI and block user
  // interactions.
}

- (void)stop {
  _webState.reset();
  [super stop];
  // TODO(crbug.com/507509954): Implement blue shimmer UI and block user
  // interactions.
}

@end
