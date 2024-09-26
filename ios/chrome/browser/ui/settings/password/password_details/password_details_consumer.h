// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class PasswordDetails;

// Sets the Password details for consumer.
@protocol PasswordDetailsConsumer <NSObject>

// Displays provided array of password details and the title for the Password
// Details view.
- (void)setPasswords:(NSArray<PasswordDetails*>*)passwords
            andTitle:(NSString*)title;

// Determine if this is a details view for a blocked site (never saved
// password).
- (void)setIsBlockedSite:(BOOL)isBlockedSite;

// Set the signed in user email.
- (void)setUserEmail:(NSString*)userEmail;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_CONSUMER_H_
