// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_COORDINATOR_DELEGATE_H_

@class AutofillBnplCoordinator;

// Delegate for AutofillBnplCoordinator.
@protocol AutofillBnplCoordinatorDelegate

// Informs the delegate that the coordinator wants to be stopped.
- (void)autofillBnplCoordinatorWantsToBeStopped:
    (AutofillBnplCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_COORDINATOR_DELEGATE_H_
