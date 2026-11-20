// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/bnpl/coordinator/autofill_bnpl_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

@interface AutofillBnplMediator () <BooleanObserver>
@end

@implementation AutofillBnplMediator {
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;
  raw_ptr<PrefService> _prefs;
  PrefBackedBoolean* _bnplEnabled;
}

- (instancetype)initWithPersonalDataManager:
                    (autofill::PersonalDataManager*)personalDataManager
                                prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(personalDataManager);
    CHECK(prefService);
    _personalDataManager = personalDataManager;
    _prefs = prefService;
    _bnplEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_prefs
                   prefName:autofill::prefs::kAutofillBnplEnabled];
    _bnplEnabled.observer = self;
  }
  return self;
}

- (void)setConsumer:(id<AutofillBnplConsumer>)consumer {
  _consumer = consumer;
  if (consumer) {
    _consumer.bnplSwitchIsOn = _bnplEnabled.value;
  }
}

- (void)disconnect {
  _personalDataManager = nullptr;
  _prefs = nullptr;
  _bnplEnabled.observer = nil;
  _bnplEnabled = nil;
}

#pragma mark - AutofillBnplTableViewControllerDelegate

- (void)viewController:(AutofillBnplTableViewController*)controller
    didChangeBnplSwitchTo:(BOOL)isOn {
  _bnplEnabled.value = isOn;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(PrefBackedBoolean*)boolean {
  CHECK_EQ(boolean, _bnplEnabled);
  self.consumer.bnplSwitchIsOn = _bnplEnabled.value;
}

@end
