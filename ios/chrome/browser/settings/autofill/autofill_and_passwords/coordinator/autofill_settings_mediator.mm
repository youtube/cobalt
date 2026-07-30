// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_settings_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_consumer.h"

@implementation AutofillSettingsMediator {
  raw_ptr<PrefService> _prefs;
  raw_ptr<signin::IdentityManager> _identityManager;
}

- (instancetype)initWithPrefService:(PrefService*)prefs
                    identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _prefs = prefs;
    _identityManager = identityManager;
  }
  return self;
}

- (void)setConsumer:(id<AutofillSettingsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  if (_consumer && _prefs && _identityManager) {
    [_consumer setAutofillAIAllowedByPolicy:
                   autofill::IsAutofillAiAllowedByEnterprisePolicy(_prefs)];
    [_consumer setEnhancedAutofillEnabled:autofill::GetAutofillAiOptInStatus(
                                              _prefs, _identityManager)];
  }
}

- (void)disconnect {
  _prefs = nullptr;
  _identityManager = nullptr;
}

#pragma mark - AutofillSettingsMutator

- (void)setEnhancedAutofillEnabled:(BOOL)enabled {
  [self.delegate autofillSettingsMediator:self
                didToggleEnhancedAutofill:enabled];
}

@end
