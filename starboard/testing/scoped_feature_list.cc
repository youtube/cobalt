// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/testing/scoped_feature_list.h"

#include "starboard/shared/starboard/feature_list.h"

namespace starboard::features {

ScopedFeatureList::~ScopedFeatureList() {
  Reset();
}

void ScopedFeatureList::InitAndEnableFeature(const SbFeature& feature) {
  InitAndEnableFeature(feature.name);
}

void ScopedFeatureList::InitAndEnableFeature(const std::string& feature_name) {
  SaveOverride(feature_name);
  FeatureList::SetFeatureForTesting(feature_name, true);
}

void ScopedFeatureList::InitAndDisableFeature(const SbFeature& feature) {
  InitAndDisableFeature(feature.name);
}

void ScopedFeatureList::InitAndDisableFeature(const std::string& feature_name) {
  SaveOverride(feature_name);
  FeatureList::SetFeatureForTesting(feature_name, false);
}

void ScopedFeatureList::SaveOverride(const std::string& feature_name) {
  for (const auto& override : overridden_features_) {
    if (override.feature_name == feature_name) {
      return;
    }
  }

  overridden_features_.push_back(
      {feature_name, FeatureList::GetOverrideForTesting(feature_name)});
}

void ScopedFeatureList::Reset() {
  for (auto it = overridden_features_.rbegin();
       it != overridden_features_.rend(); ++it) {
    if (it->value.has_value()) {
      FeatureList::SetFeatureForTesting(it->feature_name, *it->value);
    } else {
      FeatureList::ClearFeatureForTesting(it->feature_name);
    }
  }
  overridden_features_.clear();
}

}  // namespace starboard::features
