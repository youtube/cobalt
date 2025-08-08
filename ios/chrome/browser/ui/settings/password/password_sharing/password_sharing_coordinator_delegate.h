// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_DELEGATE_H_

@class PasswordSharingCoordinator;

// Delegate for PasswordSharingCoordinator.
@protocol PasswordSharingCoordinatorDelegate

// Called when the view controller was removed from navigation controller.
- (void)passwordSharingCoordinatorDidRemove:
    (PasswordSharingCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_COORDINATOR_DELEGATE_H_
