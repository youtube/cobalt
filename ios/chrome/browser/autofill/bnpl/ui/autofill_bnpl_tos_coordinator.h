// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_COORDINATOR_H_

#include <memory>

#include "base/functional/callback.h"
#import "components/autofill/core/browser/payments/bnpl_util.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Struct grouping the callbacks triggered by the ToS screen.
struct BnplCallbacks {
  base::OnceClosure accept_callback;
  base::OnceClosure cancel_callback;
};

// Coordinator managing the presentation lifecycle of the BNPL Terms of Service
// bottom sheet.
@interface AutofillBnplTosCoordinator : ChromeCoordinator

// Initializes the coordinator. `acceptCallback` and `cancelCallback` must be
// valid until this coordinator is stopped.
- (instancetype)initWithModel:
                    (std::unique_ptr<autofill::payments::BnplTosModel>)model
                    callbacks:(std::unique_ptr<BnplCallbacks>)callbacks
           baseViewController:(UIViewController*)baseViewController
                      browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_COORDINATOR_H_
