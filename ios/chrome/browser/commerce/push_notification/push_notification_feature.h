// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_PUSH_NOTIFICATION_FEATURE_H_
#define IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_PUSH_NOTIFICATION_FEATURE_H_

class ChromeBrowserState;

// Determine if the price drop notifications and ShoppingService are enabled.
// Use this function if the code embedded in the check relies on using the
// ShoppingService.
bool IsPriceTrackingEnabled(ChromeBrowserState* browser_state);

// Determine if price drop notifications are enabled. Use this function if the
// code you're guarding against is purely push notification infrastructure.
bool IsPriceNotificationsEnabled();

#endif  // IOS_CHROME_BROWSER_COMMERCE_PUSH_NOTIFICATION_PUSH_NOTIFICATION_FEATURE_H_
