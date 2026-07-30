// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_PREFS_MULTISTEP_FILTER_RETENTION_PREFS_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_PREFS_MULTISTEP_FILTER_RETENTION_PREFS_H_

#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/prefs/retention_state_snapshot.h"

class PrefRegistrySimple;
class PrefService;

namespace multistep_filter {

// Pref names for Profile-level Multistep Filter retention tracking.
inline constexpr char kRetentionSuggestionImpressionsPref[] =
    "multistep_filter.retention.suggestion_impressions_count";
inline constexpr char kRetentionSuggestionAcceptancesPref[] =
    "multistep_filter.retention.suggestion_acceptances_count";
inline constexpr char kRetentionIsLastSuggestionAcceptedPref[] =
    "multistep_filter.retention.is_last_suggestion_accepted";

// Registers Profile preferences required for Multistep Filter retention
// tracking.
void RegisterRetentionProfilePrefs(PrefRegistrySimple* registry);

// Retrieves the active retention snapshot from Profile preferences.
// Must be called on the UI thread.
RetentionStateSnapshot GetRetentionState(PrefService* pref_service);

// SET APIs:
// IMPORTANT THREADING & ACTUATION GUARANTEE:
// These functions must be called SYNCHRONOUSLY on the UI thread at the exact
// moment of UI display or user interaction. Do NOT defer calling these until
// downstream asynchronous computations finish.

// Atomically increments historical impression volume upon displaying a
// suggestion cue. Does not modify final session outcome preferences.
void RecordImpression(PrefService* pref_service);

// Atomically updates Profile retention preferences based on the user's
// interaction decision for a Multistep Filter suggestion (whether accepted,
// dismissed, ignored, or settings opened).
void RecordUserInteraction(PrefService* pref_service,
                           SuggestionUserDecision decision);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_PREFS_MULTISTEP_FILTER_RETENTION_PREFS_H_
