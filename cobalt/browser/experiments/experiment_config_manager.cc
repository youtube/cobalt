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

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/version.h"
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

ExperimentConfigManager::VersionComparisonResult
ExperimentConfigManager::CompareVersions(const std::string& version1,
                                         const std::string& version2) {
  auto parse_version = [](const std::string& v_str, int* major,
                          int* minor) -> bool {
    std::vector<std::string> parts = base::SplitString(
        v_str, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parts.size() != 3) {
      return false;
    }
    return base::StringToInt(parts[0], major) &&
           base::StringToInt(parts[2], minor);
  };

  int v1_major, v1_minor, v2_major, v2_minor;
  if (!parse_version(version1, &v1_major, &v1_minor) ||
      !parse_version(version2, &v2_major, &v2_minor)) {
    UMA_HISTOGRAM_BOOLEAN("Cobalt.Finch.VersionComparisonIsValid", false);
    return VersionComparisonResult::kInvalidFormat;
  }
  UMA_HISTOGRAM_BOOLEAN("Cobalt.Finch.VersionComparisonIsValid", true);

  if (v1_major != v2_major) {
    return v1_major > v2_major ? VersionComparisonResult::kGreaterThan
                               : VersionComparisonResult::kLessThanOrEqual;
  }

  // Major versions are equal, compare minor versions.
  return v1_minor > v2_minor ? VersionComparisonResult::kGreaterThan
                             : VersionComparisonResult::kLessThanOrEqual;
}

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

  // Check if a rollback happened. If so, apply the empty config.
  std::string recorded_cobalt_version =
      use_safe_config
          ? experiment_config_->GetString(kSafeConfigMinVersion)
          : experiment_config_->GetString(kExperimentConfigMinVersion);

  // Min version prefs are added later than other prefs, so it might be missing
  // for some users.
  if (!recorded_cobalt_version.empty() &&
      CompareVersions(recorded_cobalt_version, COBALT_VERSION) ==
          VersionComparisonResult::kGreaterThan) {
    UMA_HISTOGRAM_BOOLEAN("Cobalt.Finch.RollbackDetected", true);
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
  experiment_config_->SetString(
      kSafeConfigMinVersion,
      experiment_config_->GetString(kExperimentConfigMinVersion));
  experiment_config_->CommitPendingWrite();
  called_store_safe_config_ = true;
}

}  // namespace cobalt
