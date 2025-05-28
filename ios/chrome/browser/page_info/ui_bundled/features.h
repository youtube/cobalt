// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_FEATURES_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_FEATURES_H_

#include "base/feature_list.h"

// Feature for the implementation of Last Visited in Page Info for iOS.
BASE_DECLARE_FEATURE(kPageInfoLastVisitedIOS);

// Whether the AboutThisSite feature is enabled. Only users with languages that
// are supported should fetch the AboutThisSite information and see the UI.
bool IsAboutThisSiteFeatureEnabled();

// Whether the Last Visited feature in Page Info is enabled.
bool IsPageInfoLastVisitedIOSEnabled();

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_FEATURES_H_
