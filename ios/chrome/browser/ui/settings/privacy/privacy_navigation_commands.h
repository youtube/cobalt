// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_NAVIGATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_NAVIGATION_COMMANDS_H_

// Commands related to the privacy navigation inside the privacy view
// controller.
@protocol PrivacyNavigationCommands

// Shows Handoff screen.
- (void)showHandoff;

// Shows ClearBrowsingData screen.
- (void)showClearBrowsingData;

// Shows SafeBrowsing screen.
- (void)showSafeBrowsing;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_NAVIGATION_COMMANDS_H_
