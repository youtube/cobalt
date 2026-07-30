// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_view_controller.h"

@protocol AutofillBnplTosConsumer;

struct BnplCallbacks;

// Mediator managing the data flow and callbacks for the BNPL ToS screen.
@interface AutofillBnplTosMediator : NSObject <AutofillBnplTosMutator>

// The consumer updated by this mediator.
@property(nonatomic, weak) id<AutofillBnplTosConsumer> consumer;

// Initializes the mediator with the BNPL ToS data model and the callbacks.
- (instancetype)initWithModel:(autofill::payments::BnplTosModel)model
                    callbacks:(std::unique_ptr<BnplCallbacks>)callbacks
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TOS_MEDIATOR_H_
