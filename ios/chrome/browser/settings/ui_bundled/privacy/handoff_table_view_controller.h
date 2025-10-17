// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_HANDOFF_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_HANDOFF_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

class ProfileIOS;

// This View Controller is responsible for managing the settings related to
// Handoff.
@interface HandoffTableViewController : SettingsRootTableViewController

// The designated initializer. `profile` must not be nil.
- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_HANDOFF_TABLE_VIEW_CONTROLLER_H_
