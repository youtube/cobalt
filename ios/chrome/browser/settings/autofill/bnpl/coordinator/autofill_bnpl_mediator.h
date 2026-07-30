// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/autofill/bnpl/ui/autofill_bnpl_table_view_controller.h"

namespace autofill {
class PersonalDataManager;
}
class PrefService;

// Mediator for the Autofill BNPL settings subpage.
@interface AutofillBnplMediator
    : NSObject <AutofillBnplTableViewControllerDelegate>

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                                prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// The consumer for this mediator.
@property(nonatomic, weak) id<AutofillBnplConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_COORDINATOR_AUTOFILL_BNPL_MEDIATOR_H_
