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

#include "starboard/android/shared/starboard_test_environment.h"

#include <cstdint>
#include <iterator>

#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/features.h"

namespace starboard {
// The initialization function accepts an array of SbFeatures and
// SbFeatureParams as arguments, so these macros help create that array to pass
// to the initializer. To see more info about how these macros work and how they
// help initialize the feature list instance, please refer to
// //starboard/extension/feature_config.h for more info.
//
// We disable clang-format here as there are issues between
// clang-format and cpplint when running pre-commit on this code.
// clang-format off
#define FEATURE_LIST_START const SbFeature kStarboardFeatures[] = {
#define FEATURE_LIST_END };
#define FEATURE_PARAM_LIST_START const SbFeatureParam kStarboardParams[] = {
#define FEATURE_PARAM_LIST_END };
// clang-format on

#define STARBOARD_FEATURE(feature, name, default_state) features::feature,

#define STARBOARD_FEATURE_PARAM(T, param_object_name, feature_object_name, \
                                param_name, default_value)                 \
  features::param_object_name,

#define STARBOARD_FEATURE_PARAM_TIME_TYPE int64_t

#include "starboard/extension/feature_config.h"

#undef STARBOARD_FEATURE
#undef STARBOARD_FEATURE_PARAM
#undef FEATURE_LIST_START
#undef FEATURE_LIST_END
#undef FEATURE_PARAM_LIST_START
#undef FEATURE_PARAM_LIST_END
#undef STARBOARD_FEATURE_PARAM_TIME_TYPE

StarboardTestEnvironment::StarboardTestEnvironment() = default;

StarboardTestEnvironment::~StarboardTestEnvironment() = default;

void StarboardTestEnvironment::SetUp() {
  features::FeatureList::InitializeFeatureList(
      kStarboardFeatures, std::size(kStarboardFeatures), kStarboardParams,
      std::size(kStarboardParams));
}
}  // namespace starboard
