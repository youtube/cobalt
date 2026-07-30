// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/model/backend_promo_profile_agent.h"

#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"
#import "ios/chrome/browser/backend_promo/model/backend_promo_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@implementation BackendPromoProfileAgent

- (void)notifyServiceIfForegroundActive {
  if (self.profileState.initStage < ProfileInitStage::kFinal) {
    return;
  }
  BackendPromoService* service =
      BackendPromoServiceFactory::GetForProfile(self.profileState.profile);
  if (service) {
    service->NotifyBackendAppForegroundActive();
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  // Initialize early the BackendPromoService even if not foreground active yet.
  BackendPromoServiceFactory::GetForProfile(self.profileState.profile);

  if (profileState.foregroundActiveScene) {
    [self notifyServiceIfForegroundActive];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    [self notifyServiceIfForegroundActive];
  }
}

@end
