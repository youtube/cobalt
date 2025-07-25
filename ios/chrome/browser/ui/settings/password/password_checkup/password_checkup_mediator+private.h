// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_PRIVATE_H_

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_mediator.h"

// This is a private extension that is intended to expose otherwise private
// members for testing.
@interface PasswordCheckupMediator ()

// Returns string containing the timestamp of the `last_completed_check`
- (NSString*)formattedElapsedTimeSinceLastCheck;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_MEDIATOR_PRIVATE_H_
