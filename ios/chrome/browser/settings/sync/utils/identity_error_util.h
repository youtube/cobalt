// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_

#import <Foundation/Foundation.h>

#import "components/sync/driver/sync_service.h"

@class AccountErrorUIInfo;
namespace syncer {
class SyncService;
}
enum class SyncState;

// Returns a data object with the needed information to display the account
// error UI. Returns nil if there is no account error to display.
AccountErrorUIInfo* GetAccountErrorUIInfo(syncer::SyncService* sync_service);

// Returns true if the identity error should be indicated on the Settings
// destination in the Overflow Menu.
bool ShouldIndicateIdentityErrorInOverflowMenu(
    syncer::SyncService* sync_service);

// Returns the state of sync.
SyncState GetSyncState(syncer::SyncService* sync_service);

#endif  // IOS_CHROME_BROWSER_SETTINGS_SYNC_UTILS_IDENTITY_ERROR_UTIL_H_
