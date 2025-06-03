// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_MAIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_MAIN_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class PrivacyGuideMainCoordinator;

// Delegate for PrivacyGuideMainCoordinator.
@protocol PrivacyGuideMainCoordinatorDelegate

// Called when all view controllers are removed from navigation controller.
- (void)privacyGuideMainCoordinatorDidRemove:
    (PrivacyGuideMainCoordinator*)coordinator;

@end

// The main coordinator for the Privacy Guide. Handles the coordinators for each
// of the guide's steps.
@interface PrivacyGuideMainCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<PrivacyGuideMainCoordinatorDelegate> delegate;

@end
#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_MAIN_COORDINATOR_H_
