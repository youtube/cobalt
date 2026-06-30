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

#ifndef COBALT_BROWSER_FEATURES_H_
#define COBALT_BROWSER_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace cobalt {
namespace features {

// Enables the variations config expiration check.
extern const base::Feature kExperimentConfigExpiration;

// Test finch feature for Finch end to end testing.
extern const base::Feature kTestFinchFeature;

// Test finch feature param for Finch end to end testing.
extern const base::FeatureParam<std::string> kTestFinchFeatureParam;

}  // namespace features
}  // namespace cobalt

#endif  // COBALT_BROWSER_FEATURES_H_
