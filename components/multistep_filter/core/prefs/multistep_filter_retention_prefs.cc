// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/prefs/multistep_filter_retention_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace multistep_filter {

void RegisterRetentionProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kRetentionSuggestionImpressionsPref, 0);
  registry->RegisterIntegerPref(kRetentionSuggestionAcceptancesPref, 0);
  registry->RegisterBooleanPref(kRetentionIsLastSuggestionAcceptedPref, false);
}

RetentionStateSnapshot GetRetentionState(PrefService* pref_service) {
  RetentionStateSnapshot snapshot;
  if (!pref_service) {
    return snapshot;
  }
  snapshot.suggestion_impressions =
      pref_service->GetInteger(kRetentionSuggestionImpressionsPref);
  snapshot.suggestion_acceptances =
      pref_service->GetInteger(kRetentionSuggestionAcceptancesPref);
  snapshot.is_last_suggestion_accepted =
      pref_service->GetBoolean(kRetentionIsLastSuggestionAcceptedPref);
  return snapshot;
}

void RecordImpression(PrefService* pref_service) {
  if (!pref_service) {
    return;
  }
  const int impressions =
      pref_service->GetInteger(kRetentionSuggestionImpressionsPref);
  pref_service->SetInteger(kRetentionSuggestionImpressionsPref,
                           impressions + 1);
}

void RecordUserInteraction(PrefService* pref_service,
                           SuggestionUserDecision decision) {
  if (!pref_service) {
    return;
  }
  switch (decision) {
    case SuggestionUserDecision::kAccepted: {
      const int acceptances =
          pref_service->GetInteger(kRetentionSuggestionAcceptancesPref);
      pref_service->SetInteger(kRetentionSuggestionAcceptancesPref,
                               acceptances + 1);
      pref_service->SetBoolean(kRetentionIsLastSuggestionAcceptedPref, true);
      break;
    }
    case SuggestionUserDecision::kDismissed:
    case SuggestionUserDecision::kIgnored:
    case SuggestionUserDecision::kSettingsOpened:
      pref_service->SetBoolean(kRetentionIsLastSuggestionAcceptedPref, false);
      break;
  }
}

}  // namespace multistep_filter
