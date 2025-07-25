// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_constants.h"
#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_consumer.h"
#import "ios/chrome/browser/ui/unit_conversion/unit_conversion_mutator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"

@implementation UnitConversionMediator {
  // A boolean to track if the unit type has changed.
  BOOL _unitTypeChanged;
  // A boolean to track if the source unit value has changed.
  BOOL _sourceUnitValueChanged;

  // An item to track the source unit change before the unit type is changed,
  // it's initialised with the value `kUnchanged` and store the first change
  // only.
  UnitConversionActionTypes _sourceUnitChangedBeforeUnitType;

  // An item to track the source unit change after the unit type is changed,
  // it's initialised with the value `kUnchanged` and store the first change
  // only.
  UnitConversionActionTypes _sourceUnitChangedAfterUnitType;

  // An item to track the target unit change, it is initialised with the value
  // `kUnchanged` and store the first change only.
  UnitConversionActionTypes _targetUnitChanged;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _unitTypeChanged = NO;
    _sourceUnitValueChanged = NO;
    _sourceUnitChangedBeforeUnitType = UnitConversionActionTypes::kUnchanged;
    _sourceUnitChangedAfterUnitType = UnitConversionActionTypes::kUnchanged;
    _targetUnitChanged = UnitConversionActionTypes::kUnchanged;
  }
  return self;
}

- (void)reportMetrics {
  base::UmaHistogramEnumeration(kSourceUnitChangeAfterUnitTypeChangeHistogram,
                                _sourceUnitChangedAfterUnitType);
  base::UmaHistogramEnumeration(kSourceUnitChangeBeforeUnitTypeChangeHistogram,
                                _sourceUnitChangedBeforeUnitType);
  base::UmaHistogramEnumeration(kTargetUnitChangeHistogram, _targetUnitChanged);
}

#pragma mark - UnitConversionMutator

- (void)unitTypeDidChange:(ios::provider::UnitType)unitType
                unitValue:(double)unitValue {
  NSUnit* sourceUnit = ios::provider::GetDefaultUnitForType(unitType);
  NSUnit* targetUnit = ios::provider::GetDefaultTargetUnit(sourceUnit);

  if (sourceUnit && targetUnit) {
    NSMeasurement* sourceUnitMeasurement =
        [[NSMeasurement alloc] initWithDoubleValue:unitValue unit:sourceUnit];
    if ([sourceUnitMeasurement canBeConvertedToUnit:targetUnit]) {
      if (!_unitTypeChanged) {
        base::RecordAction(
            base::UserMetricsAction("IOS.UnitConversion.UnitTypeChange"));
        _unitTypeChanged = YES;
      }
      NSMeasurement* targetUnitMeasurement =
          [sourceUnitMeasurement measurementByConvertingToUnit:targetUnit];

      [self.consumer updateSourceUnit:sourceUnit reload:NO];
      [self.consumer updateTargetUnit:targetUnit reload:NO];
      [self.consumer updateSourceUnitValue:sourceUnitMeasurement.doubleValue
                                    reload:NO];
      [self.consumer updateTargetUnitValue:targetUnitMeasurement.doubleValue
                                    reload:NO];
      [self.consumer updateUnitTypeTitle:unitType];
    }
  }
}

- (void)sourceUnitDidChange:(NSUnit*)sourceUnit
                 targetUnit:(NSUnit*)targetUnit
                  unitValue:(double)unitValue
                   unitType:(ios::provider::UnitType)unitType {
  NSMeasurement* sourceUnitMeasurement =
      [[NSMeasurement alloc] initWithDoubleValue:unitValue unit:sourceUnit];
  if ([sourceUnitMeasurement canBeConvertedToUnit:targetUnit]) {
    NSMeasurement* targetUnitMeasurement =
        [sourceUnitMeasurement measurementByConvertingToUnit:targetUnit];

    [self.consumer updateSourceUnit:sourceUnit reload:YES];
    [self.consumer updateTargetUnitValue:targetUnitMeasurement.doubleValue
                                  reload:YES];
    if (_unitTypeChanged) {
      // Update _sourceUnitChangedAfterUnitType only if the unit type has
      // changed and its previous value is `kUnchanged`.
      if (_sourceUnitChangedAfterUnitType ==
          UnitConversionActionTypes::kUnchanged) {
        _sourceUnitChangedAfterUnitType =
            [self unitConversionActionFromUnitType:unitType
                                          isMetric:NSLocale.currentLocale
                                                       .usesMetricSystem];
      }
    } else {
      // Update `_sourceUnitChangedBeforeUnitType` only if the unit type hasn't
      // changed and its previous value is `kUnchanged`.
      if (_sourceUnitChangedBeforeUnitType ==
          UnitConversionActionTypes::kUnchanged) {
        _sourceUnitChangedBeforeUnitType =
            [self unitConversionActionFromUnitType:unitType
                                          isMetric:NSLocale.currentLocale
                                                       .usesMetricSystem];
      }
    }
  }
}

- (void)targetUnitDidChange:(NSUnit*)targetUnit
                 sourceUnit:(NSUnit*)sourceUnit
                  unitValue:(double)unitValue
                   unitType:(ios::provider::UnitType)unitType {
  NSMeasurement* sourceUnitMeasurement =
      [[NSMeasurement alloc] initWithDoubleValue:unitValue unit:sourceUnit];
  if ([sourceUnitMeasurement canBeConvertedToUnit:targetUnit]) {
    NSMeasurement* targetUnitMeasurement =
        [sourceUnitMeasurement measurementByConvertingToUnit:targetUnit];

    [self.consumer updateTargetUnit:targetUnit reload:YES];
    [self.consumer updateTargetUnitValue:targetUnitMeasurement.doubleValue
                                  reload:YES];
    // update `_targetUnitChanged` only if its value is `kUnchanged`.
    if (_targetUnitChanged == UnitConversionActionTypes::kUnchanged) {
      _targetUnitChanged =
          [self unitConversionActionFromUnitType:unitType
                                        isMetric:NSLocale.currentLocale
                                                     .usesMetricSystem];
    }
  }
}

- (void)sourceUnitValueFieldDidChange:(NSString*)sourceUnitValueField
                           sourceUnit:(NSUnit*)sourceUnit
                           targetUnit:(NSUnit*)targetUnit {
  NSNumber* unitValueNumber = [self numberFromString:sourceUnitValueField];
  if (!unitValueNumber) {
    return;
  }
  double unitValue = unitValueNumber.doubleValue;
  NSMeasurement* sourceUnitMeasurement =
      [[NSMeasurement alloc] initWithDoubleValue:unitValue unit:sourceUnit];
  if ([sourceUnitMeasurement canBeConvertedToUnit:targetUnit]) {
    NSMeasurement* targetUnitMeasurement =
        [sourceUnitMeasurement measurementByConvertingToUnit:targetUnit];
    [self.consumer updateTargetUnitValue:targetUnitMeasurement.doubleValue
                                  reload:YES];

    // Record only the first sourceUnitValueChange before any change of the unit
    // type.
    if (!_unitTypeChanged && !_sourceUnitValueChanged) {
      _sourceUnitValueChanged = YES;
      base::RecordAction(
          base::UserMetricsAction("IOS.UnitConversion.SourceUnitValueChange"));
    }
  }
}

#pragma mark - Private

// Returns a `UnitConversionActionTypes` based on the `UnitType` and the
// current locale system (metric/imperial).
- (UnitConversionActionTypes)unitConversionActionFromUnitType:
                                 (ios::provider::UnitType)unitType
                                                     isMetric:(BOOL)isMetric {
  switch (unitType) {
    case ios::provider::kUnitTypeArea:
      return isMetric ? UnitConversionActionTypes::kAreaMetric
                      : UnitConversionActionTypes::kAreaImperial;
    case ios::provider::kUnitTypeInformationStorage:
      return isMetric ? UnitConversionActionTypes::kInformationStorageMetric
                      : UnitConversionActionTypes::kInformationStorageImperial;
    case ios::provider::kUnitTypeLength:
      return isMetric ? UnitConversionActionTypes::kLengthMetric
                      : UnitConversionActionTypes::kLengthImperial;
    case ios::provider::kUnitTypeMass:
      return isMetric ? UnitConversionActionTypes::kMassMetric
                      : UnitConversionActionTypes::kMassImperial;
    case ios::provider::kUnitTypeSpeed:
      return isMetric ? UnitConversionActionTypes::kSpeedMetric
                      : UnitConversionActionTypes::kSpeedImperial;
    case ios::provider::kUnitTypeTemperature:
      return isMetric ? UnitConversionActionTypes::kTemperatureMetric
                      : UnitConversionActionTypes::kTemperatureImperial;
    case ios::provider::kUnitTypeVolume:
      return isMetric ? UnitConversionActionTypes::kVolumeMetric
                      : UnitConversionActionTypes::kVolumeImperial;
    case ios::provider::kUnitTypeUnknown:
      NOTREACHED_NORETURN();
  }
}

// Converts a string to a NSNumber*, returns nil if not valid.
- (NSNumber*)numberFromString:(NSString*)string {
  NSNumberFormatter* numberFormatter = [[NSNumberFormatter alloc] init];
  numberFormatter.locale = [NSLocale currentLocale];
  numberFormatter.numberStyle = NSNumberFormatterDecimalStyle;
  return [numberFormatter numberFromString:string];
}

@end
