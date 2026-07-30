// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_PUBLIC_ACCOUNT_MENU_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_PUBLIC_ACCOUNT_MENU_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Possible "access points" from where the account menu can be triggered.
// Enums for UMA Signin.IOSAccountMenu.Opened.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IOSAccountMenuAccessPoint)
enum class AccountMenuAccessPoint {
  // The most common one: The account particle disc on the NTP.
  kNewTabPage = 0,
  // The "Use another account" button in Chrome settings.
  kSettings = 1,
  // A button on a Gaia web page, called something like "Use another account" or
  // "Manage accounts".
  kWeb = 2,
  // The account button in the app bar.
  kAppBar = 3,
  // The account menu triggered from the Page Action Menu.
  kPageActionMenu = 4,
  // Presented from the Gemini entry flow when the signed-in account
  // is ineligible due to Gemini policy restriction.
  kGeminiEntryFlow = 5,
  // The account menu triggered from the overflow menu.
  kOverflowMenu = 6,
  kMaxValue = kOverflowMenu,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:IOSAccountMenuAccessPoint)

// The accessibility identifier of the view controller's view.
extern NSString* const kAccountMenuTableViewId;
// The accessibility identifier of the view controller's close button.
extern NSString* const kAccountMenuCloseButtonId;
// The accessibility identifier of the view controller's ellipsis button.
extern NSString* const kAccountMenuSecondaryActionMenuButtonId;
// The accessibility identifier of the Error message.
extern NSString* const kAccountMenuErrorMessageId;
// The accessibility identifier of the Error button.
extern NSString* const kAccountMenuErrorActionButtonId;
// The accessibility identifier of the Add Account button.
extern NSString* const kAccountMenuAddAccountButtonId;
// The accessibility identifier of the Sign out.
extern NSString* const kAccountMenuSignoutButtonId;
// The accessibility identifier for the secondary accounts buttons.
extern NSString* const kAccountMenuSecondaryAccountButtonId;
// The accessibility identifier for the account menu activity indicator.
extern NSString* const kAccountMenuActivityIndicatorId;
// The accessibility identifier of manage accounts button.
extern NSString* const kAccountMenuManageAccountsButtonId;
// The accessibility identifier for the "edit account list" menu entry.
extern NSString* const kAccountMenuEditAccountListId;
// The accessibility identifier for the "manage your account" menu entry.
extern NSString* const kAccountMenuManageYourGoogleAccountId;

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_PUBLIC_ACCOUNT_MENU_CONSTANTS_H_
