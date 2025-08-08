// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_mediator.h"

#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"

@interface MagicStackHalfSheetMediator () <BooleanObserver>
@end

@implementation MagicStackHalfSheetMediator {
  PrefService* _prefService;

  PrefBackedBoolean* _setUpListDisabled;
  PrefBackedBoolean* _safetyCheckDisabled;
  PrefBackedBoolean* _tabResumptionDisabled;
  PrefBackedBoolean* _parcelTrackingDisabled;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  if (self = [super init]) {
    CHECK(prefService);
    _prefService = prefService;
    if (IsIOSSetUpListEnabled() &&
        set_up_list_utils::IsSetUpListActive(_prefService, false)) {
      _setUpListDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_prefService
                     prefName:set_up_list_prefs::kDisabled];
      [_setUpListDisabled setObserver:self];
    }
    if (IsSafetyCheckMagicStackEnabled()) {
      _safetyCheckDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_prefService
                     prefName:safety_check_prefs::
                                  kSafetyCheckInMagicStackDisabledPref];
      [_safetyCheckDisabled setObserver:self];
    }
    if (IsTabResumptionEnabled()) {
      _tabResumptionDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_prefService
                     prefName:tab_resumption_prefs::kTabResumptioDisabledPref];
      [_tabResumptionDisabled setObserver:self];
    }
    if (IsIOSParcelTrackingEnabled()) {
      _parcelTrackingDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_prefService
                     prefName:kParcelTrackingDisabled];
      [_parcelTrackingDisabled setObserver:self];
    }
  }
  return self;
}

- (void)disconnect {
  _prefService = nil;
  if (_setUpListDisabled) {
    [_setUpListDisabled setObserver:nil];
    _setUpListDisabled = nil;
  }
}

- (void)setConsumer:(id<MagicStackHalfSheetConsumer>)consumer {
  _consumer = consumer;
  if (_setUpListDisabled) {
    [self.consumer showSetUpList:YES];
    [self.consumer setSetUpListDisabled:_setUpListDisabled.value];
  }
  if (_safetyCheckDisabled) {
    [self.consumer setSafetyCheckDisabled:_safetyCheckDisabled.value];
  }
  if (_tabResumptionDisabled) {
    [self.consumer setTabResumptionDisabled:_tabResumptionDisabled.value];
  }
  if (_parcelTrackingDisabled) {
    [self.consumer setParcelTrackingDisabled:_parcelTrackingDisabled.value];
  }
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _setUpListDisabled) {
    [self.consumer setSetUpListDisabled:_setUpListDisabled.value];
  } else if (observableBoolean == _safetyCheckDisabled) {
    [self.consumer setSafetyCheckDisabled:_safetyCheckDisabled.value];
  } else if (observableBoolean == _tabResumptionDisabled) {
    [self.consumer setTabResumptionDisabled:_tabResumptionDisabled.value];
  } else if (observableBoolean == _parcelTrackingDisabled) {
    [self.consumer setParcelTrackingDisabled:_parcelTrackingDisabled.value];
  }
}

#pragma mark - MagicStackHalfSheetDelegate

- (void)setUpListEnabledChanged:(BOOL)setUpListEnabled {
  [_setUpListDisabled setValue:!setUpListEnabled];
}

- (void)safetyCheckEnabledChanged:(BOOL)safetyCheckEnabled {
  [_safetyCheckDisabled setValue:!safetyCheckEnabled];
}

- (void)tabResumptionEnabledChanged:(BOOL)tabResumptionEnabled {
  [_tabResumptionDisabled setValue:!tabResumptionEnabled];
}

- (void)parcelTrackingEnabledChanged:(BOOL)parcelTrackingEnabled {
  [_parcelTrackingDisabled setValue:!parcelTrackingEnabled];
}

@end
