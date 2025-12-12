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

#ifndef COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_FEATURES_H_
#define COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

extern const base::Feature kOomIntervention;
extern const base::FeatureParam<double> kPhysicalMemoryThresholdRatio;
extern const base::FeatureParam<int> kDetectionWindowSeconds;
extern const base::FeatureParam<double> kSwapMemoryThresholdRatio;

}  // namespace features

#endif  // COBALT_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_FEATURES_H_
