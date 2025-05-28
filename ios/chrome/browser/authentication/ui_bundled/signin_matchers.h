// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_MATCHERS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_MATCHERS_H_

#import <Foundation/Foundation.h>

@protocol GREYAction;
@protocol GREYMatcher;

namespace chrome_test_util {

// Returns a matcher for a TableViewIdentityCell based on the `email`.
id<GREYMatcher> IdentityCellMatcherForEmail(NSString* email);

// Returns a matcher for the skip button in the web sign-in consistency dialog.
id<GREYMatcher> WebSigninSkipButtonMatcher();

// Returns a matcher for the primary button in the web sign-in consistency
// dialog.
id<GREYMatcher> WebSigninPrimaryButtonMatcher();

// Matcher for the sign-in screens (like history sync opt-in, upgrade promo…).
id<GREYMatcher> SigninScreenPromoMatcher();

// Matcher for the Settings row which, upon tap, leads the user to sign-in. The
// row is only shown to signed-out users.
id<GREYMatcher> SettingsSignInRowMatcher();

// Matcher for the history opt-in screen.
id<GREYMatcher> HistoryOptInPromoMatcher();

// Action for searching an UI element in the history opt-in screen..
id<GREYAction> HistoryOptInScrollDown();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_MATCHERS_H_
