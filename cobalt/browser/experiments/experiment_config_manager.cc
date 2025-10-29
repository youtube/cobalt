// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/experiments/experiment_config_manager.h"

#include <iostream>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace cobalt {

namespace {

// Default threshold for variations config expiration in days. A negative value
// indicates that the check is disabled.
constexpr int kDefaultExpirationThresholdInDays = -1;

// An enum for the UMA histogram to track the state of the variations config.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VariationsConfigState {
  kValid = 0,
  kExpired = 1,
  kMissingTimestamp = 2,
  kMaxValue = kMissingTimestamp,
};

// Checks if the experiment config has expired by reading the last fetch time.
bool HasConfigExpired(PrefService* experiment_prefs) {
  // If the pref is missing, we cannot determine its age. For backward
  // compatibility and safety on first run, we treat it as valid
  if (!experiment_prefs->HasPrefPath(
          variations::prefs::kVariationsLastFetchTime)) {
    UMA_HISTOGRAM_ENUMERATION("Cobalt.Finch.ConfigState",
                              VariationsConfigState::kMissingTimestamp);
    return false;
  }

  base::Time fetch_time =
      experiment_prefs->GetTime(variations::prefs::kVariationsLastFetchTime);
  base::TimeDelta config_age = base::Time::Now() - fetch_time;

  UMA_HISTOGRAM_CUSTOM_COUNTS("Cobalt.Finch.ConfigAgeInDays",
                              config_age.InDays(), 1, 90, 50);

  // Get the expiration threshold from the server config.
  const int expiration_threshold_in_days =
      experiment_prefs->GetDict(kFinchParameters)
          .FindInt("experiment_expiration_threshold_days")
          .value_or(kDefaultExpirationThresholdInDays);

  if (expiration_threshold_in_days >= 0 &&
      config_age.InDays() > expiration_threshold_in_days) {
    LOG(WARNING) << "Variations config from " << fetch_time
                 << " has expired. Ignoring.";
    UMA_HISTOGRAM_ENUMERATION("Cobalt.Finch.ConfigState",
                              VariationsConfigState::kExpired);
    return true;
  }

  UMA_HISTOGRAM_ENUMERATION("Cobalt.Finch.ConfigState",
                            VariationsConfigState::kValid);
  return false;
}

}  // namespace

ExperimentConfigManager::ExperimentConfigManager(
    PrefService* experiment_config,
    PrefService* metrics_local_state)
    : experiment_config_(experiment_config),
      metrics_local_state_(metrics_local_state) {
  DCHECK(experiment_config_);
  DCHECK(metrics_local_state_);
}

ExperimentConfigType ExperimentConfigManager::GetExperimentConfigType() {
  // First, determine the config type based on the crash streak.
  DCHECK(metrics_local_state_);
  DCHECK(!called_store_safe_config_);
  int num_crashes = metrics_local_state_->GetInteger(
      variations::prefs::kVariationsCrashStreak);
  static_assert(
      kCrashStreakEmptyConfigThreshold > kCrashStreakSafeConfigThreshold,
      "Threshold to use an empty experiment config should be larger "
      "than to use the safe one.");
  std::cout << "num_crashes" << num_crashes << std::endl;
  if (num_crashes >= kCrashStreakEmptyConfigThreshold) {
    return ExperimentConfigType::kEmptyConfig;
  }

  ExperimentConfigType config_type;
  if (num_crashes >= kCrashStreakSafeConfigThreshold) {
    config_type = ExperimentConfigType::kSafeConfig;
  } else {
    config_type = ExperimentConfigType::kRegularConfig;
  }

  // Now, check if the expiration feature is enabled in the determined config.
  // This check reads directly from the pref because it runs *before* the
  // global base::FeatureList is initialized.
  const bool use_safe_config =
      (config_type == ExperimentConfigType::kSafeConfig);
  const base::Value::Dict& feature_map = experiment_config_->GetDict(
      use_safe_config ? kSafeConfigFeatures : kExperimentConfigFeatures);
  const bool expiration_enabled =
      feature_map.FindBool(features::kExperimentConfigExpiration.name)
          .value_or(false);

  // If the feature is enabled and the config is expired, override the result to
  // treat it as an empty config.
  if (HasConfigExpired(experiment_config_) && expiration_enabled) {
    return ExperimentConfigType::kEmptyConfig;
  }
  return config_type;
}

void ExperimentConfigManager::StoreSafeConfig() {
  // This should only be called upon the first setExperimentState() call from
  // h5vcc. Active config would be stored as the safe config.
  // Note that it's sufficient to do this only upon receiving the first
  // experiment config from h5vcc call because the active configuration does
  // not change while Cobalt is running. Also, note that it's a noop if running
  // in safe mode.
  DCHECK(experiment_config_);
  if (called_store_safe_config_) {
    return;
  }
  auto experiment_config_type = GetExperimentConfigType();
  if (experiment_config_type != ExperimentConfigType::kRegularConfig) {
    return;
  }

  experiment_config_->SetDict(
      kSafeConfigFeatures,
      experiment_config_->GetDict(kExperimentConfigFeatures).Clone());
  experiment_config_->SetDict(
      kSafeConfigFeatureParams,
      experiment_config_->GetDict(kExperimentConfigFeatureParams).Clone());
  experiment_config_->SetString(
      kSafeConfigActiveConfigData,
      experiment_config_->GetString(kExperimentConfigActiveConfigData));
  experiment_config_->CommitPendingWrite();
  called_store_safe_config_ = true;
}

}  // namespace cobalt
