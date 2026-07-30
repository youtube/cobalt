// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AutofillBnplCoordinatorDelegate;

// Coordinator for the BNPL (Buy Now Pay Later) settings subpage.
@interface AutofillBnplCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:
                    (UINavigationController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// The delegate for this coordinator.
@property(nonatomic, weak) id<AutofillBnplCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_COORDINATOR_H_
