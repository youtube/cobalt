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

#ifndef COBALT_BROWSER_CONSTANTS_COBALT_EXPERIMENT_NAMES_H_
#define COBALT_BROWSER_CONSTANTS_COBALT_EXPERIMENT_NAMES_H_

namespace cobalt {

// Alphabetical list of experiment names in the Cobalt experiment config.
constexpr char kCobaltExperimentName[] = "CobaltExperiment";
constexpr char kCobaltGroupName[] = "CobaltGroup";
// Apply empty config if crash streak exceeds this threshold.
constexpr int kCrashStreakEmptyConfigThreshold = 4;
// Apply safe config if crash streak exceeds this threshold.
constexpr int kCrashStreakSafeConfigThreshold = 3;
constexpr char kExperimentConfig[] = "experiment_config";
constexpr char kExperimentConfigActiveConfigData[] =
    "experiment_config.active_config_data";
constexpr char kExperimentConfigFeatures[] = "experiment_config.features";
constexpr char kExperimentConfigFeatureParams[] =
    "experiment_config.feature_params";
constexpr char kExperimentConfigMinVersion[] = "experiment_config.min_version";
constexpr char kFinchParameters[] = "finch_parameters";
constexpr char kLatestConfigHash[] = "latest_config_hash";
constexpr char kSafeConfig[] = "safe_config";
constexpr char kSafeConfigActiveConfigData[] = "safe_config.active_config_data";
constexpr char kSafeConfigFeatures[] = "safe_config.features";
constexpr char kSafeConfigFeatureParams[] = "safe_config.feature_params";
constexpr char kSafeConfigMinVersion[] = "safe_config.min_version";

}  // namespace cobalt

#endif  // COBALT_BROWSER_CONSTANTS_COBALT_EXPERIMENT_NAMES_H_
