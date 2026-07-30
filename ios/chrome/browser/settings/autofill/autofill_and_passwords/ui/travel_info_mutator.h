// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_MUTATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_mutator.h"

// Mutator protocol for the Travel Info settings page.
@protocol TravelInfoMutator <AutofillAIBaseMutator>

// Notifies the mutator that the user toggled the "save and fill travel info" switch.
- (void)didToggleTravelInfo:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_TRAVEL_INFO_MUTATOR_H_
