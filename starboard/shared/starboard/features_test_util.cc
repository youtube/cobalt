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

#include "starboard/shared/starboard/features_test_util.h"

#include <stdint.h>

#include <string>

#include "starboard/configuration.h"
#include "starboard/extension/features.h"
#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/features.h"

namespace starboard::features {

namespace {

// `starboard/extension/feature_config.h` expresses time-parameter defaults as
// `base::Microseconds(...)`, `base::Milliseconds(...)`, and
// `base::Seconds(...)`, while test targets under Starboard must not depend on
// `//base`. Provide file-local constexpr helpers inside this anonymous
// namespace so default parameter values from `feature_config.h` are preserved
// in unit tests.
namespace base {

[[maybe_unused]] constexpr int64_t Microseconds(int64_t n) {
  return n;
}
[[maybe_unused]] constexpr int64_t Milliseconds(int64_t n) {
  return n * 1000;
}
[[maybe_unused]] constexpr int64_t Seconds(int64_t n) {
  return n * 1000000;
}

}  // namespace base

[[maybe_unused]] SbFeatureParam MakeDefaultParam(
    const SbFeatureParamExt<bool>& base,
    bool default_value) {
  SbFeatureParam param = base;
  param.value.bool_value = default_value;
  return param;
}

[[maybe_unused]] SbFeatureParam MakeDefaultParam(
    const SbFeatureParamExt<int>& base,
    int default_value) {
  SbFeatureParam param = base;
  param.value.int_value = default_value;
  return param;
}

[[maybe_unused]] SbFeatureParam MakeDefaultParam(
    const SbFeatureParamExt<double>& base,
    double default_value) {
  SbFeatureParam param = base;
  param.value.double_value = default_value;
  return param;
}

[[maybe_unused]] SbFeatureParam MakeDefaultParam(
    const SbFeatureParamExt<std::string>& base,
    const char* default_value) {
  SbFeatureParam param = base;
  param.value.string_value = default_value;
  return param;
}

[[maybe_unused]] SbFeatureParam MakeDefaultParam(
    const SbFeatureParamExt<int64_t>& base,
    int64_t default_value) {
  SbFeatureParam param = base;
  param.value.time_value = default_value;
  return param;
}

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

#define STARBOARD_FEATURE(feature, name, default_state) feature,

#define STARBOARD_FEATURE_PARAM(T, param_object_name, feature, name, \
                                default_value)                       \
  MakeDefaultParam(param_object_name, default_value),

#define STARBOARD_FEATURE_PARAM_TIME_TYPE int64_t

#include "starboard/extension/feature_config.h"

#undef STARBOARD_FEATURE
#undef STARBOARD_FEATURE_PARAM
#undef FEATURE_LIST_START
#undef FEATURE_LIST_END
#undef FEATURE_PARAM_LIST_START
#undef FEATURE_PARAM_LIST_END
#undef STARBOARD_FEATURE_PARAM_TIME_TYPE

}  // namespace

void InitializeStarboardFeatureListWithDefaults() {
  FeatureList::InitializeFeatureList(
      kStarboardFeatures, SB_ARRAY_SIZE(kStarboardFeatures), kStarboardParams,
      SB_ARRAY_SIZE(kStarboardParams));
}

}  // namespace starboard::features
