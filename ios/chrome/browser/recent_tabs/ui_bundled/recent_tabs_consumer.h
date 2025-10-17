// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_RECENT_TABS_CONSUMER_H_
#define IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_RECENT_TABS_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/recent_tabs/ui_bundled/sessions_sync_user_state.h"

namespace sessions {
class TabRestoreService;
}

// RecentTabs Consumer interface.
@protocol RecentTabsConsumer <NSObject>

// Refreshes the table view to match the current sync state.
- (void)refreshUserState:(SessionsSyncUserState)state;

// Refreshes the recently closed tab section.
- (void)refreshRecentlyClosedTabs;

// Sets the service used to populate the closed tab section. Can be used to nil
// the service in case it is not available anymore.
- (void)setTabRestoreService:(sessions::TabRestoreService*)tabRestoreService;

// Dismisses any outstanding modal user interface elements.
- (void)dismissModals;

@end

#endif  // IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_RECENT_TABS_CONSUMER_H_
