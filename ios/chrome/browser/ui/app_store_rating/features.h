// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_APP_STORE_RATING_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_APP_STORE_RATING_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable the App Store Rating feature.
BASE_DECLARE_FEATURE(kAppStoreRating);

// Returns true if App Store Rating feature is enabled.
bool IsAppStoreRatingEnabled();

#endif  // IOS_CHROME_BROWSER_UI_APP_STORE_RATING_FEATURES_H_
