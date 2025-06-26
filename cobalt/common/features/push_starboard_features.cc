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

#include "cobalt/common/features/push_starboard_features.h"

#include <string>
#include <utility>
#include <variant>

#include "base/strings/string_number_conversions.h"
#include "cobalt/common/features/features.h"
#include "starboard/extension/features.h"
#include "starboard/system.h"

namespace cobalt::features {
namespace {

// Helper function to deduce what type the value is at compile time.
template <typename T>
constexpr SbFeatureParamType GetParamType() {
  if constexpr (std::is_same_v<T, bool>) {
    return SbFeatureParamTypeBool;
  } else if constexpr (std::is_same_v<T, int>) {
    return SbFeatureParamTypeInt;
  } else if constexpr (std::is_same_v<T, size_t>) {
    return SbFeatureParamTypeSize;
  } else if constexpr (std::is_same_v<T, double>) {
    return SbFeatureParamTypeDouble;
  } else if constexpr (std::is_same_v<T, const char*>) {
    return SbFeatureParamTypeString;
  } else if constexpr (std::is_same_v<T, int64_t>) {
    return SbFeatureParamTypeTime;
  } else {  // Throw a compile error if an unsupported type is used.
    static_assert(!std::is_same_v<T, T>,
                  "Unsupported type for SbParamWrapper. Must be one of: bool, "
                  "int, size_t, double, const char*, int64_t");
  }
}

struct SbParamWrapper {
  typedef std::variant<bool, std::string, int, double, size_t, int64_t>
      ParamValue;

  const base::Feature* feature;
  const char* feature_name;
  const char* param_name;
  SbFeatureParamType type;
  ParamValue default_value;

  template <typename T>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           T value)
      : feature(feature),
        feature_name(feature->name),
        param_name(name),
        type(GetParamType<T>()),
        default_value(value) {}

  void TransferFeatureParamValue(SbFeatureParam& sbparam,
                                 std::string param_value) const {
    switch (type) {
      case (SbFeatureParamTypeBool):
        sbparam.bool_value = base::GetFieldTrialParamByFeatureAsBool(
            *feature, param_name, std::get<bool>(default_value));
        break;
      case (SbFeatureParamTypeInt):
        sbparam.int_value = base::GetFieldTrialParamByFeatureAsInt(
            *feature, param_name, std::get<int>(default_value));
        break;
      case (SbFeatureParamTypeSize):
        if (!base::StringToSizeT(param_value, &sbparam.size_value)) {
          sbparam.size_value = std::get<size_t>(default_value);
        }
        break;
      case (SbFeatureParamTypeDouble):
        sbparam.double_value = base::GetFieldTrialParamByFeatureAsDouble(
            *feature, param_name, std::get<double>(default_value));
        break;
      case (SbFeatureParamTypeString):
        sbparam.string_value =
            param_value == "" ? (std::get<std::string>(default_value).c_str())
                              : param_value.c_str();
        break;
      case (SbFeatureParamTypeTime):
        if (!base::StringToInt64(param_value, &sbparam.time_value)) {
          sbparam.time_value = std::get<int64_t>(default_value);
        }
    }
  }
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

#include "starboard/common/feature_config.h"

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
    std::string current_value = base::GetFieldTrialParamValueByFeature(
        *(param.feature), param.param_name);
    SbFeatureParam SbParam = {(param.feature)->name, param.param_name,
                              param.type};
    param.TransferFeatureParamValue(SbParam, current_value);
    params.push_back(SbParam);
  }

  extension_api->InitializeStarboardFeatures(features.data(), features.size(),
                                             params.data(), params.size());
}
}  // namespace cobalt::features
