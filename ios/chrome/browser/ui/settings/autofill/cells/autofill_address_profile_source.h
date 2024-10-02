// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_ADDRESS_PROFILE_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_ADDRESS_PROFILE_SOURCE_H_

#import <Foundation/Foundation.h>

// Corresponds to `autofill::AutofillProfile::Source` with the exception that
// `autofill::AutofillProfile::Source::kLocalOrSyncable` is further broken into
// local and syncable profiles.
typedef NS_ENUM(NSInteger, AutofillAddressProfileSource) {
  AutofillAccountProfile,
  AutofillSyncableProfile,
  AutofillLocalProfile
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_CELLS_AUTOFILL_ADDRESS_PROFILE_SOURCE_H_
