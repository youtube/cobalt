// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_FEATURES_H_
#define IOS_CHROME_BROWSER_WEB_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace web {

// Feature flag to enable lockdown mode within browser.
BASE_DECLARE_FEATURE(kEnableBrowserLockdownMode);

// Returns true if the use of Browser Lockdown Mode is enabled.
bool IsBrowserLockdownModeEnabled();

// Feature flag to tie the default zoom level for webpages to the current
// dynamic type setting.
BASE_DECLARE_FEATURE(kWebPageDefaultZoomFromDynamicType);

// Used to enable a different method of zooming web pages.
BASE_DECLARE_FEATURE(kWebPageAlternativeTextZoom);

// Reneables text zoom on iPad.
BASE_DECLARE_FEATURE(kWebPageTextZoomIPad);

// Feature flag for to use native session restoration.
BASE_DECLARE_FEATURE(kRestoreSessionFromCache);

}  // namespace web

#endif  // IOS_CHROME_BROWSER_WEB_FEATURES_H_
