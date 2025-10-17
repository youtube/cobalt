// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/docking_promo_profile_agent.h"

#import <optional>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"

@implementation DockingPromoProfileAgent

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  switch (DockingPromoExperimentTypeEnabled()) {
    case DockingPromoDisplayTriggerArm::kDuringFRE:
      break;
    case DockingPromoDisplayTriggerArm::kAfterFRE:
    case DockingPromoDisplayTriggerArm::kAppLaunch:
      [self maybeRegisterPromo];
      break;
  }

  [profileState removeObserver:self];
  [profileState removeAgent:self];
}

#pragma mark - Private

// Register the promo with the PromosManager, if the conditions are met.
- (void)maybeRegisterPromo {
  if (IsDockingPromoForcedForDisplay()) {
    [self registerPromo];
    return;
  }

  const base::TimeDelta timeSinceLastForeground =
      MinTimeSinceLastForeground(self.profileState.foregroundScenes)
          .value_or(base::TimeDelta::Min());

  if (!CanShowDockingPromo(timeSinceLastForeground)) {
    if (!base::FeatureList::IsEnabled(
            kIOSDockingPromoPreventDeregistrationKillswitch)) {
      [self deregisterPromo];
    }
    return;
  }

  // Record that the user has at least been found eligible once
  // for the docking promo. This means that it is now possible
  // to use IsDockingPromoForEligibleUsersOnlyEnabled() and be
  // sure that the feature will only be A/B tested on users that
  // are eligible, thus reducing the noise in measurement by not
  // including ineligible users.
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kIosDockingPromoEligibilityMet, true);

  if (IsDockingPromoForEligibleUsersOnlyEnabled()) {
    [self registerPromo];
  }
}

// Registers the Docking Promo with the PromosManager.
- (void)registerPromo {
  DCHECK(self.profileState.profile);
  PromosManagerFactory::GetForProfile(self.profileState.profile)
      ->RegisterPromoForSingleDisplay(promos_manager::Promo::DockingPromo);
}

// Deregisters the Docking Promo from the PromosManager.
- (void)deregisterPromo {
  DCHECK(self.profileState.profile);
  PromosManagerFactory::GetForProfile(self.profileState.profile)
      ->DeregisterPromo(promos_manager::Promo::DockingPromo);
}

@end
