// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"

#include "base/json/values_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace optimization_guide::model_execution::prefs {

namespace {

struct LegacyUsagePref {
  const char* path;
  ModelBasedCapabilityKey feature;
};

constexpr LegacyUsagePref kLegacyUsagePrefs[] = {
    {"optimization_guide.last_time_on_device_eligible_feature_used",
     ModelBasedCapabilityKey::kCompose},
    {"optimization_guide.model_execution.last_time_prompt_api_used",
     ModelBasedCapabilityKey::kPromptApi},
    {"optimization_guide.model_execution.last_time_summarize_api_used",
     ModelBasedCapabilityKey::kSummarize},
    {"optimization_guide.model_execution.last_time_test_used",
     ModelBasedCapabilityKey::kTest},
    {"optimization_guide.model_execution.last_time_history_search_used",
     ModelBasedCapabilityKey::kHistorySearch},
    {"optimization_guide.model_execution.last_time_history_query_intent_used",
     ModelBasedCapabilityKey::kHistoryQueryIntent},
};

std::string PrefKey(ModelBasedCapabilityKey key) {
  return base::NumberToString(
      (static_cast<uint64_t>(ToModelExecutionFeatureProto(key))));
}

void SetLastUsage(PrefService* local_state,
                  ModelBasedCapabilityKey feature,
                  base::Time time) {
  ::prefs::ScopedDictionaryPrefUpdate update(local_state,
                                             localstate::kLastUsageByFeature);
  update->Set(PrefKey(feature), base::TimeToValue(time));
}

bool IsUseRecent(std::optional<base::Time> last_use) {
  if (!last_use) {
    return false;
  }
  auto time_since_use = base::Time::Now() - *last_use;
  base::TimeDelta recent_use_period =
      features::GetOnDeviceEligibleModelFeatureRecentUsePeriod();
  // Note: Since we're storing a base::Time, we need to consider the possibility
  // of clock changes.
  return time_since_use < recent_use_period &&
         time_since_use > -recent_use_period;
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  RegisterGenAiFeatures(registry);
}

namespace localstate {

// Preference of the last version checked. Used to determine when the
// disconnect count is reset.
const char kOnDeviceModelChromeVersion[] =
    "optimization_guide.on_device.last_version";

// Preference where number of disconnects (crashes) of on device model is
// stored.
const char kOnDeviceModelCrashCount[] =
    "optimization_guide.on_device.model_crash_count";

const char kOnDeviceModelValidationResult[] =
    "optimization_guide.on_device.model_validation_result";

// Stores the last computed `OnDeviceModelPerformanceClass` of the device.
const char kOnDevicePerformanceClass[] =
    "optimization_guide.on_device.performance_class";

// Stores the last chrome version that the performance class was checked.
const char kOnDevicePerformanceClassVersion[] =
    "optimization_guide.on_device.performance_class_version";

// Timestamps for the last time each features was used while on-device eligible.
// Used to decide which models are worth fetching.
const char kLastUsageByFeature[] =
    "optimization_guide.model_execution.last_usage_by_feature";

// A timestamp for the last time the on-device model was eligible for download.
const char kLastTimeEligibleForOnDeviceModelDownload[] =
    "optimization_guide.on_device.last_time_eligible_for_download";

// An integer pref that contains the user's client id.
const char kModelQualityLogggingClientId[] =
    "optimization_guide.model_quality_logging_client_id";

// An integer pref for the on-device GenAI foundational model enterprise policy
// settings.
const char kGenAILocalFoundationalModelEnterprisePolicySettings[] =
    "optimization_guide.gen_ai_local_foundational_model_settings";

}  // namespace localstate

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(localstate::kOnDeviceModelChromeVersion,
                               std::string());
  registry->RegisterIntegerPref(localstate::kOnDeviceModelCrashCount, 0);
  registry->RegisterIntegerPref(localstate::kOnDevicePerformanceClass, 0);
  registry->RegisterStringPref(localstate::kOnDevicePerformanceClassVersion,
                               std::string());
  registry->RegisterTimePref(
      localstate::kLastTimeEligibleForOnDeviceModelDownload, base::Time::Min());
  registry->RegisterDictionaryPref(localstate::kOnDeviceModelValidationResult);
  registry->RegisterDictionaryPref(localstate::kLastUsageByFeature);
  registry->RegisterInt64Pref(localstate::kModelQualityLogggingClientId, 0,
                              PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      localstate::kGenAILocalFoundationalModelEnterprisePolicySettings, 0);
}

void RegisterLegacyUsagePrefsForMigration(PrefRegistrySimple* registry) {
  for (auto& pref : kLegacyUsagePrefs) {
    registry->RegisterTimePref(pref.path, base::Time::Min());
  }
}

void MigrateLegacyUsagePrefs(PrefService* local_state) {
  for (auto& pref : kLegacyUsagePrefs) {
    if (local_state->HasPrefPath(pref.path)) {
      DCHECK(!local_state->GetDict(localstate::kLastUsageByFeature)
                  .Find(PrefKey(pref.feature)));
      SetLastUsage(local_state, pref.feature, local_state->GetTime(pref.path));
      local_state->ClearPref(pref.path);
    }
  }
}

void PruneOldUsagePrefs(PrefService* local_state) {
  ::prefs::ScopedDictionaryPrefUpdate update(local_state,
                                             localstate::kLastUsageByFeature);
  std::vector<std::string> keys_to_prune_;  // Avoid iterator invalidation.
  for (auto kv : *update->AsConstDict()) {
    if (!IsUseRecent(base::ValueToTime(kv.second))) {
      keys_to_prune_.emplace_back(kv.first);
    }
  }
  for (const auto& key : keys_to_prune_) {
    update->Remove(key);
  }
}

void RecordFeatureUsage(PrefService* local_state,
                        ModelBasedCapabilityKey feature) {
  SetLastUsage(local_state, feature, base::Time::Now());
}

bool WasFeatureRecentlyUsed(const PrefService* local_state,
                            ModelBasedCapabilityKey feature) {
  const auto* value = local_state->GetDict(localstate::kLastUsageByFeature)
                          .Find(PrefKey(feature));
  if (!value) {
    return false;
  }
  return IsUseRecent(base::ValueToTime(*value));
}

}  // namespace optimization_guide::model_execution::prefs
