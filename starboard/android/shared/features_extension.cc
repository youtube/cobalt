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

#include "starboard/android/shared/features_extension.h"

#include "starboard/extension/features.h"
#include "starboard/shared/starboard/feature_list.h"

namespace starboard::android::shared {
// Extension function to receive the features and parameters passed
// down from Cobalt. This function will then pass these values to
// Starboard's own FeatureList class, where they can be initialized
// for use under Starboard.
void InitializeStarboardFeatures(const SbFeature* features,
                                 size_t number_of_features,
                                 const SbFeatureParam* params,
                                 size_t number_of_params) {
  starboard::features::FeatureList::InitializeFeatureList(
      features, number_of_features, params, number_of_params);
}

constexpr StarboardExtensionFeaturesApi kFeaturesApi = {
    kStarboardExtensionFeaturesName,
    1u,
    &InitializeStarboardFeatures,
};

const void* GetFeaturesApi() {
  return &kFeaturesApi;
}

}  // namespace starboard::android::shared
