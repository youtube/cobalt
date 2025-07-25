// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_FEATURES_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable skipping evaluation of the Fullscreen Promos Manager's
// internal Impression Limits.
BASE_DECLARE_FEATURE(kFullscreenPromosManagerSkipInternalLimits);

// Controls whether the Promos Manager should use the Feature Engagement Tracker
// as its impression tracking system.
BASE_DECLARE_FEATURE(kPromosManagerUsesFET);

// Returns true if the Fullscreen Promos Manager should skip evaluation of its
// internal Impression Limits when deciding whether or not to display a promo.
bool IsSkippingInternalImpressionLimitsEnabled();

bool ShouldPromosManagerUseFET();

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_FEATURES_H_
