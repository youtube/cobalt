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

#include "base/logging.h"
#include "cobalt/browser/constants/cobalt_experiment_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace cobalt {

ExperimentConfigType ExperimentConfigManager::GetExperimentConfigType() {
  DCHECK(experiment_config_);
  DCHECK(!called_store_safe_config_);
  int num_crashes =
      experiment_config_->GetInteger(variations::prefs::kVariationsCrashStreak);
  static_assert(
      kCrashStreakEmptyConfigThreshold > kCrashStreakSafeConfigThreshold,
      "Threshold to use an empty experiment config should be larger "
      "than to use the safe one.");
  if (num_crashes >= kCrashStreakEmptyConfigThreshold) {
    return ExperimentConfigType::kEmptyConfig;
  }
  if (num_crashes >= kCrashStreakSafeConfigThreshold) {
    return ExperimentConfigType::kSafeConfig;
  }
  return ExperimentConfigType::kRegularConfig;
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
  experiment_config_->SetList(
      kSafeConfigExpIds,
      experiment_config_->GetList(kExperimentConfigExpIds).Clone());
  experiment_config_->CommitPendingWrite();
  called_store_safe_config_ = true;
}

}  // namespace cobalt
