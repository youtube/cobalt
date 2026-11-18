// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_SIGNIN_PROMO_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_SIGNIN_PROMO_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_consumer.h"

@class SigninPromoViewConfigurator;
@protocol SigninPromoViewDelegate;

// Consumer protocol for the Autofill and Passwords sign-in promo mediator.
@protocol AutofillAndPasswordsSigninPromoConsumer <SigninPromoViewConsumer>

// Sets the delegate for sign-in promo view events.
- (void)setSigninPromoDelegate:
    (id<SigninPromoViewDelegate>)signinPromoDelegate;

// Controls the visibility state of the sign-in promo.
- (void)promoStateChanged:(BOOL)promoEnabled
        promoConfigurator:(SigninPromoViewConfigurator*)promoConfigurator
                promoText:(NSString*)promoText;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_AUTOFILL_AND_PASSWORDS_SIGNIN_PROMO_CONSUMER_H_
