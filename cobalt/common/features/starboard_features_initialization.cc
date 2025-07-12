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

#include "cobalt/common/features/starboard_features_initialization.h"

#include <string>
#include <utility>
#include <variant>

#include "base/strings/string_number_conversions.h"
#include "cobalt/common/features/features.h"
#include "starboard/extension/features.h"
#include "starboard/system.h"

namespace cobalt::features {
namespace {
// Helper struct to assemble SbFeatureParam objects.
struct SbParamWrapper {
  const base::Feature* feature;
  const char* param_name;
  SbFeatureParamType type;
  SbFeatureParam::ParamValue value;

  template <typename T>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           T param_value) {
    static_assert(!std::is_same_v<T, T>,
                  "Unsupported type for SbParamWrapper. Must be one of: bool, "
                  "int, double, std::string, int64_t");
  }

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           bool param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeBool),
        value({.bool_value = param_value}) {}

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           int param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeInt),
        value({.int_value = param_value}) {}

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           double param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeDouble),
        value({.double_value = param_value}) {}

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           std::string param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeString),
        value({.string_value = param_value.c_str()}) {}

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           base::TimeDelta param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeTime),
        value({.time_value = param_value.InSeconds()}) {}
};

SbFeatureParam::ParamValue GetParamValue(const SbParamWrapper& wrapper) {
  SbFeatureParam::ParamValue value;
  switch (wrapper.type) {
    case SbFeatureParamTypeBool:
      value.bool_value = base::GetFieldTrialParamByFeatureAsBool(
          *(wrapper.feature), wrapper.param_name, wrapper.value.bool_value);
      return value;
    case SbFeatureParamTypeInt:
      value.int_value = base::GetFieldTrialParamByFeatureAsInt(
          *(wrapper.feature), wrapper.param_name, wrapper.value.int_value);
      return value;
    case SbFeatureParamTypeDouble:
      value.double_value = base::GetFieldTrialParamByFeatureAsDouble(
          *(wrapper.feature), wrapper.param_name, wrapper.value.double_value);
      return value;
    case SbFeatureParamTypeString:
      value.string_value = base::GetFieldTrialParamValueByFeature(
                               *(wrapper.feature), wrapper.param_name) == ""
                               ? (wrapper.value.string_value)
                               : base::GetFieldTrialParamValueByFeature(
                                     *(wrapper.feature), wrapper.param_name)
                                     .c_str();
      return value;
    case SbFeatureParamTypeTime:
      if (!base::StringToInt64(base::GetFieldTrialParamValueByFeature(
                                   *(wrapper.feature), wrapper.param_name),
                               &(value.time_value))) {
        value.time_value = wrapper.value.time_value;
      }
      return value;
  }
}
}  // namespace

#define FEATURE_LIST_START \
  constexpr const base::Feature* kStarboardFeatures[] = {
#define FEATURE_LIST_END \
  }                      \
  ;
#define FEATURE_PARAM_LIST_START const SbParamWrapper kStarboardParams[] = {
#define FEATURE_PARAM_LIST_END \
  }                            \
  ;

#define STARBOARD_FEATURE(feature, name, default_state) &feature,

#define STARBOARD_FEATURE_PARAM(T, param_object_name, feature_object_name, \
                                param_name, default_value)                 \
  SbParamWrapper(&feature_object_name, param_name,                         \
                 static_cast<T>(default_value)),

#define TIME base::TimeDelta

#include "starboard/extension/feature_config.h"

#undef TIME
#undef STARBOARD_FEATURE
#undef STARBOARD_FEATURE_PARAM
#undef FEATURE_LIST_START
#undef FEATURE_LIST_END
#undef FEATURE_PARAM_LIST_START
#undef FEATURE_PARAM_LIST_END

// This function should be called right after Finch experiment states
// are loaded.
void InitializeStarboardFeatures() {
  const StarboardExtensionFeaturesApi* extension_api =
      static_cast<const StarboardExtensionFeaturesApi*>(
          SbSystemGetExtension(kStarboardExtensionFeaturesName));
  if (!extension_api) {
    return;
  }

  DCHECK_EQ(extension_api->name, std::string(kStarboardExtensionFeaturesName));
  DCHECK_EQ(extension_api->version, 1u);
  DCHECK(extension_api->InitializeStarboardFeatures);

  std::vector<SbFeature> features;
  std::vector<SbFeatureParam> params;

  for (const base::Feature* feature : kStarboardFeatures) {
    features.push_back(
        SbFeature{feature->name, base::FeatureList::IsEnabled(*(feature))});
  }

  for (const SbParamWrapper& param : kStarboardParams) {
    params.push_back(SbFeatureParam{param.feature->name, param.param_name,
                                    param.type, GetParamValue(param)});
  }

  extension_api->InitializeStarboardFeatures(features.data(), features.size(),
                                             params.data(), params.size());
}
}  // namespace cobalt::features
