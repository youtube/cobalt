// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"
#include "base/time/time.h"

// Key for the tab group in grid inactive duration testing override.
extern NSString* const kShowTabGroupInGridInactiveDurationKey;

// Returns the inactive duration to show the tab group in grid view.
base::TimeDelta GetReturnToTabGroupInGridDuration();

// The feature parameter to indicate inactive duration to return to the Start
// Surface in seconds.
extern const char kReturnToStartSurfaceInactiveDurationInSeconds[];

// The feature to gate the Start Surface user setting toggle.
BASE_DECLARE_FEATURE(kStartSurfaceUserSetting);

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_FEATURES_H_
