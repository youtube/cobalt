// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_PUBLIC_AUTOFILL_SETTINGS_NAVIGATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_PUBLIC_AUTOFILL_SETTINGS_NAVIGATOR_H_

#import <Foundation/Foundation.h>

// Pages in the Settings UI that can be navigated to from Autofill surfaces.
enum class AutofillSettingsPage {
  kPasswordManager,
  kPasswordSettings,
  kCreditCards,
  kAddresses,
};

// Delegate protocol for handling navigation to settings pages.
@protocol AutofillSettingsNavigator <NSObject>

// Requests to open the settings page for the specified `page`.
- (void)openSettingsForPage:(AutofillSettingsPage)page;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_PUBLIC_AUTOFILL_SETTINGS_NAVIGATOR_H_
