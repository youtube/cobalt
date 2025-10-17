// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_MUTATOR_H_

namespace browsing_data {
enum class TimePeriod;
}

// Mutator for the QuickDeleteViewController to update the QuickDeleteMediator.
@protocol QuickDeleteMutator <NSObject>

// Called when the user selects a `timeRange` for the deletion of browsing data.
- (void)timeRangeSelected:(browsing_data::TimePeriod)timeRange;

// Called when the user decides to go through with the deletion.
// Does nothing if the current scene is blocked.
- (void)triggerDeletionIfPossible;

// Called on confirming the browsing data types selection with the user choice
// for the history type.
- (void)updateHistorySelection:(BOOL)selected;

// Called on confirming the browsing data types selection with the user choice
// for the tabs type.
- (void)updateTabsSelection:(BOOL)selected;

// Called on confirming the browsing data types selection with the user choice
// for the site data type.
- (void)updateSiteDataSelection:(BOOL)selected;

// Called on confirming the browsing data types selection with the user choice
// for the cache type.
- (void)updateCacheSelection:(BOOL)selected;

// Called on confirming the browsing data types selection with the user choice
// for the passwords type.
- (void)updatePasswordsSelection:(BOOL)selected;

// Called on confirming the browsing data types selection with the user choice
// for the autofill type.
- (void)updateAutofillSelection:(BOOL)selected;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_MUTATOR_H_
