// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"

#import <Foundation/Foundation.h>

#import "base/metrics/field_trial_params.h"

namespace {
// Default value for kDefaultShowTabGridInactiveDurationInSeconds.
constexpr base::TimeDelta kDefaultShowTabGroupInGridInactiveDuration =
    base::Hours(1);
}  // anonymous namespace

// Key for the tab group in grid inactive duration testing override.
NSString* const kShowTabGroupInGridInactiveDurationKey =
    @"ShowTabGroupInGridInactiveDuration";

const char kReturnToStartSurfaceInactiveDurationInSeconds[] =
    "ReturnToStartSurfaceInactiveDurationInSeconds";

base::TimeDelta GetReturnToTabGroupInGridDuration() {
  NSNumber* testing_override = [[NSUserDefaults standardUserDefaults]
      objectForKey:kShowTabGroupInGridInactiveDurationKey];
  if (testing_override) {
    return base::Seconds([testing_override doubleValue]);
  }
  return kDefaultShowTabGroupInGridInactiveDuration;
}

BASE_FEATURE(kStartSurfaceUserSetting, base::FEATURE_DISABLED_BY_DEFAULT);
