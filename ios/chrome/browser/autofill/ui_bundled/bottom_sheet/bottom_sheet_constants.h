// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_BOTTOM_SHEET_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_BOTTOM_SHEET_CONSTANTS_H_

#import <UIKit/UIKit.h>

#import "base/time/time.h"

// Accessibility identifier of the Edit Address Cancel Button.
extern NSString* const kEditProfileBottomSheetCancelButton;

// Accessibility identifier for the Edit Address Bottom Sheet.
extern NSString* const kEditProfileBottomSheetViewIdentfier;

// Table view cell identifier for Save Card and Virtual Card Enrollment Bottom
// Sheet.
extern NSString* const kDetailIconCellIdentifier;

// Time duration to wait before auto dismissing the Save Card or Virtual Card
// Enrollment Bottom Sheet in success confirmation state.
extern base::TimeDelta const kConfirmationDismissDelay;

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_BOTTOM_SHEET_CONSTANTS_H_
