// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_mutator.h"

@protocol AutofillSettingsConsumer;
@class AutofillSettingsMediator;
class PrefService;
namespace signin {
class IdentityManager;
}

// Delegate for AutofillSettingsMediator.
@protocol AutofillSettingsMediatorDelegate <NSObject>

// Notifies the delegate that the user toggled the Enhanced Autofill switch.
- (void)autofillSettingsMediator:(AutofillSettingsMediator*)mediator
       didToggleEnhancedAutofill:(BOOL)enabled;

@end

@protocol ReauthenticationProtocol;

// Mediator for the Autofill settings page.
@interface AutofillSettingsMediator : NSObject <AutofillSettingsMutator>

// Consumer for this mediator.
@property(nonatomic, weak) id<AutofillSettingsConsumer> consumer;

// Delegate for this mediator.
@property(nonatomic, weak) id<AutofillSettingsMediatorDelegate> delegate;

- (instancetype)initWithPrefService:(PrefService*)prefs
                    identityManager:(signin::IdentityManager*)identityManager
             reauthenticationModule:(id<ReauthenticationProtocol>)reauthModule
              shouldShowWalletPromo:(BOOL)shouldShowWalletPromo
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_SETTINGS_MEDIATOR_H_
