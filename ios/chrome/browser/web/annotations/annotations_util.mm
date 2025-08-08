// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/annotations/annotations_util.h"

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/common/features.h"

bool IsAddressDetectionEnabled() {
  if (@available(iOS 16.4, *)) {
    return base::FeatureList::IsEnabled(web::features::kOneTapForMaps);
  }
  return false;
}

bool IsAddressAutomaticDetectionEnabled(PrefService* prefs) {
  return IsAddressDetectionEnabled() &&
         prefs->GetBoolean(prefs::kDetectAddressesEnabled);
}

bool IsAddressAutomaticDetectionAccepted(PrefService* prefs) {
  return IsAddressDetectionEnabled() &&
         prefs->GetBoolean(prefs::kDetectAddressesAccepted);
}

bool ShouldPresentConsentIPH(PrefService* prefs) {
  std::string param = base::GetFieldTrialParamValueByFeature(
      web::features::kOneTapForMaps,
      web::features::kOneTapForMapsConsentModeParamTitle);
  if (param == web::features::kOneTapForMapsConsentModeIPHForcedParam) {
    return true;
  }
  if (param == web::features::kOneTapForMapsConsentModeIPHParam) {
    return !IsAddressAutomaticDetectionAccepted(prefs);
  }
  return false;
}

bool ShouldPresentConsentScreen(PrefService* prefs) {
  std::string param = base::GetFieldTrialParamValueByFeature(
      web::features::kOneTapForMaps,
      web::features::kOneTapForMapsConsentModeParamTitle);
  if (param == web::features::kOneTapForMapsConsentModeForcedParam) {
    return true;
  }
  if (param == web::features::kOneTapForMapsConsentModeDisabledParam ||
      param == web::features::kOneTapForMapsConsentModeIPHParam ||
      param == web::features::kOneTapForMapsConsentModeIPHForcedParam) {
    return false;
  }
  return !IsAddressAutomaticDetectionAccepted(prefs);
}

bool IsAddressLongPressDetectionEnabled(PrefService* prefs) {
  return !IsAddressDetectionEnabled() ||
         prefs->GetBoolean(prefs::kDetectAddressesEnabled);
}
