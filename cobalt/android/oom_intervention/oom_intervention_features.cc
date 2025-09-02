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

#include "cobalt/android/oom_intervention/oom_intervention_features.h"

namespace features {

BASE_FEATURE(kOomIntervention,
             "OomIntervention",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<double> kPhysicalMemoryThresholdRatio{
    &kOomIntervention, "physical_memory_threshold_ratio", 0.45};

const base::FeatureParam<int> kDetectionWindowSeconds{
    &kOomIntervention, "detection_window_seconds", 60};

const base::FeatureParam<double> kSwapMemoryThresholdRatio{
    &kOomIntervention, "swap_memory_threshold_ratio", 0.45};

}  // namespace features
