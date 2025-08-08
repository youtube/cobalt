// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_PRIVATE_H_

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"

// This is a private extension that is intended to expose otherwise private
// members for testing.
@interface PasswordDetailsMediator ()

// The credentials to be displayed in the page.
@property(nonatomic, assign) std::vector<password_manager::CredentialUIEntry>
    credentials;

// The context in which the password details are accessed.
@property(nonatomic, assign) DetailsContext context;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_MEDIATOR_PRIVATE_H_
