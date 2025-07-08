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
typedef std::variant<bool, int, double, std::string, int64_t> WrapperValue;

// Helper struct to assemble SbFeatureParam objects. This structure
// will deduce the corresponding param value and its type on initialization.
struct SbParamWrapper {
  const base::Feature* feature;
  const char* param_name;
  SbFeatureParamType type;
  WrapperValue value;

  // Template constructors for the supported types. If a type
  // is not supported, a compiler error is thrown.
  template <typename T>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           T param_value) {
    static_assert(!std::is_same_v<T, T>,
                  "Unsupported type for SbParamWrapper. Must be one of: bool, "
                  "int, double, std::string, base::TimeDelta");
  }

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           bool param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeBool),
        value(param_value) {}

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           int param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeInt),
        value(param_value) {}

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           double param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeDouble),
        value(param_value) {}

  template <>
  SbParamWrapper(const base::Feature* feature,
                 const char* name,
                 std::string param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeString),
        value(param_value) {}

  template <>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           base::TimeDelta param_value)
      : feature(feature),
        param_name(name),
        type(SbFeatureParamTypeTime),
        value(param_value.InSeconds()) {}
};

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

  for (const SbParamWrapper& wrapper : kStarboardParams) {
    SbFeatureParam param{wrapper.feature->name, wrapper.param_name,
                         wrapper.type};

    // Assign the value of the SbFeatureParam based on type. We do this inside
    // the InitializeStarboardFeatures function as if the parameter is a string,
    // we must ensure that the string passed to Starboard lives until
    // the end of initialization.
    switch (wrapper.type) {
      case SbFeatureParamTypeBool:
        param.value.bool_value = base::GetFieldTrialParamByFeatureAsBool(
            *(wrapper.feature), wrapper.param_name,
            std::get<bool>(wrapper.value));
        break;
      case SbFeatureParamTypeInt:
        param.value.int_value = base::GetFieldTrialParamByFeatureAsInt(
            *(wrapper.feature), wrapper.param_name,
            std::get<int>(wrapper.value));
        break;
      case SbFeatureParamTypeDouble:
        param.value.double_value = base::GetFieldTrialParamByFeatureAsDouble(
            *(wrapper.feature), wrapper.param_name,
            std::get<double>(wrapper.value));
        break;
      case SbFeatureParamTypeString: {
        std::string overridden_value = base::GetFieldTrialParamValueByFeature(
            *(wrapper.feature), wrapper.param_name);
        if (!overridden_value.empty()) {
          param.value.string_value = overridden_value.c_str();
        } else {
          param.value.string_value =
              std::get<std::string>(wrapper.value).c_str();
        }
        break;
      }
      case SbFeatureParamTypeTime:
        if (!base::StringToInt64(base::GetFieldTrialParamValueByFeature(
                                     *(wrapper.feature), wrapper.param_name),
                                 &(param.value.time_value))) {
          param.value.time_value = std::get<int64_t>(wrapper.value);
        }
    }
    params.push_back(param);
  }

  extension_api->InitializeStarboardFeatures(features.data(), features.size(),
                                             params.data(), params.size());
}
}  // namespace cobalt::features
