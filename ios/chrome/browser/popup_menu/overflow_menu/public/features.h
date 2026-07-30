// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POPUP_MENU_OVERFLOW_MENU_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_POPUP_MENU_OVERFLOW_MENU_PUBLIC_FEATURES_H_

#import "base/feature.h"

// Enables the OverflowMenuNtpRefactor feature. Removes some menu items from the
// overflow menu on the NTP and disables long pressing on menu items on the NTP.
// Moves the NTP entrypoint for homepage customization from the NTP header to
// the overflow menu.
BASE_DECLARE_FEATURE(kOverflowMenuNTPRefactor);

// Returns true if the OverflowMenuNtpRefactor feature is enabled.
bool IsOverflowMenuNTPRefactorEnabled();

// Enables the OverflowMenuHomeCustomizationEntrypoint feature.
BASE_DECLARE_FEATURE(kOverflowMenuHomeCustomizationEntrypoint);

// Returns true if the OverflowMenuHomeCustomizationEntrypoint feature is
// enabled.
bool IsOverflowMenuHomeCustomizationEntrypointEnabled();

#endif  // IOS_CHROME_BROWSER_POPUP_MENU_OVERFLOW_MENU_PUBLIC_FEATURES_H_
