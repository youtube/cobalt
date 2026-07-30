// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_CONSUMER_H_

#import <UIKit/UIKit.h>

// Holds data for a single bullet point item in the ToS screen.
@interface AutofillBnplTosBulletItem : NSObject

// The text content of the bullet point.
@property(nonatomic, copy) NSAttributedString* text;

// The icon image shown next to the text.
@property(nonatomic, strong) UIImage* icon;

@end

// Consumer protocol for the BNPL Terms of Service screen.
@protocol AutofillBnplTosConsumer <NSObject>

// Sets the title text of the bottom sheet.
- (void)setTitleText:(NSString*)titleText;

// Sets the Issuer logo image to display in the header.
- (void)setIssuerLogo:(UIImage*)issuerLogo;

// Sets the bullet point terms/benefits to display.
- (void)setTermsBulletPoints:(NSArray<AutofillBnplTosBulletItem*>*)bulletPoints;

// Sets the long legal consent message text (attributed with links).
- (void)setConsentText:(NSAttributedString*)consentText;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_CONSUMER_H_
