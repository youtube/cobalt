// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/utils_test_support.h"

#import "ios/chrome/browser/default_browser/model/utils.h"

// Visible for testing.
extern NSString* const kDefaultBrowserUtilsKey;

void ClearDefaultBrowserPromoData() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  for (NSString* key in DefaultBrowserUtilsLegacyKeysForTesting()) {
    [defaults removeObjectForKey:key];
  }
  [defaults removeObjectForKey:kDefaultBrowserUtilsKey];
}

void ResetStorageAndSetObjectForKey(NSString* key, NSObject* data) {
  NSMutableDictionary<NSString*, NSObject*>* dict =
      [[NSMutableDictionary alloc] init];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  dict[key] = data;

  [defaults setObject:dict forKey:kDefaultBrowserUtilsKey];
}

void SetValuesInStorage(NSDictionary<NSString*, NSObject*>* dict) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:dict forKey:kDefaultBrowserUtilsKey];
}
