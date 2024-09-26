// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/flags/ios_chrome_field_trials.h"

#import "base/check.h"
#import "base/path_service.h"
#import "components/metrics/persistent_histograms.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_field_trial.h"
#import "ios/chrome/browser/ui/ntp/synced_segments_field_trial.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void IOSChromeFieldTrials::OnVariationsSetupComplete() {
  // Persistent histograms must be enabled ASAP, but depends on Features.
  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    InstantiatePersistentHistogramsWithFeaturesAndCleanup(user_data_dir);
  } else {
    NOTREACHED();
  }
}

void IOSChromeFieldTrials::SetUpClientSideFieldTrials(
    bool has_seed,
    const variations::EntropyProviders& entropy_providers,
    base::FeatureList* feature_list) {
  // Disable trials when testing to remove sources of nondeterminism.
  // WARNING: Do not add any field trials until after this check, or
  // else they will be incorrectly randomized during EG testing.
  if (tests_hook::DisableClientSideFieldTrials()) {
    return;
  }

  new_tab_page_field_trial::Create(entropy_providers.low_entropy(),
                                   feature_list,
                                   GetApplicationContext()->GetLocalState());
  synced_segments_field_trial::Create(entropy_providers.low_entropy(),
                                      feature_list,
                                      GetApplicationContext()->GetLocalState());
}
