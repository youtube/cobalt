// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_DELEGATE_H_

@class SwitchProfileSettingsItem;

// Delegate to handle Switch Profile Settings actions.
@protocol SwitchProfileSettingsDelegate

// Opens a new window with the selected profile.
- (void)openProfileInNewWindowWithSwitchProfileSettingsItem:
    (SwitchProfileSettingsItem*)switchProfileSettingsItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_DELEGATE_H_
