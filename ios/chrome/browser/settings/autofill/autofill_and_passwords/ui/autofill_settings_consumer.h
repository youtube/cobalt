// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for the Autofill settings page.
@protocol AutofillSettingsConsumer <NSObject>

// Sets whether Enhanced Autofill is enabled.
- (void)setEnhancedAutofillEnabled:(BOOL)enabled;

// Sets whether Autofill AI is allowed by enterprise policy.
- (void)setAutofillAIAllowedByPolicy:(BOOL)allowed;

// Sets whether user verification is enabled before filling sensitive data.
- (void)setUserVerificationEnabled:(BOOL)enabled;

// Sets whether the user verification switch is enabled.
- (void)setUserVerificationSwitchEnabled:(BOOL)enabled;

// Sets whether the user verification setting is visible.
- (void)setUserVerificationSettingVisible:(BOOL)visible;

// Sets whether the Google Wallet promotion should be shown.
- (void)setShouldShowWalletPromo:(BOOL)shouldShowWalletPromo;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_SETTINGS_CONSUMER_H_
