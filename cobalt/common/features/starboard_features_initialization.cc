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
#include "starboard/common/log.h"
#include "starboard/extension/features.h"
#include "starboard/system.h"

namespace cobalt::features {
namespace {
typedef std::variant<bool, int, double, std::string, base::TimeDelta>
    WrapperValue;

// Helper struct to assemble SbFeatureParam objects. This structure will deduce
// the corresponding param value and its type on initialization.
struct SbParamWrapper {
  const base::Feature* feature;
  const char* param_name;
  SbFeatureParamType type;
  WrapperValue default_value;

  // Template constructors for the supported types. If a type is not supported,
  // a compiler error is thrown.
  template <typename T>
  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* name,
                           T param_value) {
    static_assert(!std::is_same_v<T, T>,
                  "Unsupported type for SbParamWrapper. Must be one of: bool, "
                  "int, double, std::string, base::TimeDelta");
  }

  // Template getter function to retrieve the most up-to-date value for
  // parameters.
  template <typename T>
  T GetValue() const;
};

// Specialized template constructors for the 5 supported types.
template <>
constexpr SbParamWrapper::SbParamWrapper(const base::Feature* feature,
                                         const char* name,
                                         bool param_value)
    : feature(feature),
      param_name(name),
      type(SbFeatureParamTypeBool),
      default_value(param_value) {}

template <>
constexpr SbParamWrapper::SbParamWrapper(const base::Feature* feature,
                                         const char* name,
                                         int param_value)
    : feature(feature),
      param_name(name),
      type(SbFeatureParamTypeInt),
      default_value(param_value) {}

template <>
constexpr SbParamWrapper::SbParamWrapper(const base::Feature* feature,
                                         const char* name,
                                         double param_value)
    : feature(feature),
      param_name(name),
      type(SbFeatureParamTypeDouble),
      default_value(param_value) {}

template <>
SbParamWrapper::SbParamWrapper(const base::Feature* feature,
                               const char* name,
                               std::string param_value)
    : feature(feature),
      param_name(name),
      type(SbFeatureParamTypeString),
      default_value(param_value) {}

template <>
constexpr SbParamWrapper::SbParamWrapper(const base::Feature* feature,
                                         const char* name,
                                         base::TimeDelta param_value)
    : feature(feature),
      param_name(name),
      type(SbFeatureParamTypeTime),
      default_value(param_value) {}

// Specialized GetValue() definitions for the 5 supported types.
template <>
bool SbParamWrapper::GetValue() const {
  return base::GetFieldTrialParamByFeatureAsBool(*feature, param_name,
                                                 std::get<bool>(default_value));
}

template <>
int SbParamWrapper::GetValue() const {
  return base::GetFieldTrialParamByFeatureAsInt(*feature, param_name,
                                                std::get<int>(default_value));
}

template <>
double SbParamWrapper::GetValue() const {
  return base::GetFieldTrialParamByFeatureAsDouble(
      *feature, param_name, std::get<double>(default_value));
}

template <>
std::string SbParamWrapper::GetValue() const {
  return base::GetFieldTrialParamValueByFeature(*feature, param_name);
}

template <>
base::TimeDelta SbParamWrapper::GetValue() const {
  return (base::GetFieldTrialParamByFeatureAsTimeDelta(
      *feature, param_name, std::get<base::TimeDelta>(default_value)));
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

#undef STARBOARD_FEATURE
#undef STARBOARD_FEATURE_PARAM
#undef FEATURE_LIST_START
#undef FEATURE_LIST_END
#undef FEATURE_PARAM_LIST_START
#undef FEATURE_PARAM_LIST_END
#undef TIME

void InitializeStarboardFeatures() {
  const StarboardExtensionFeaturesApi* extension_api =
      static_cast<const StarboardExtensionFeaturesApi*>(
          SbSystemGetExtension(kStarboardExtensionFeaturesName));
  if (!extension_api) {
    LOG(ERROR) << "Unable to retrieve StarboardExtension "
                  "kStarboardExtensionFeaturesName."
               << "No features or parameters have been pushed to Starboard.";
    return;
  }

  DCHECK_EQ(extension_api->name, std::string(kStarboardExtensionFeaturesName));
  DCHECK_EQ(extension_api->version, 1u);
  DCHECK(extension_api->InitializeStarboardFeatures);

  std::vector<SbFeature> features;
  std::vector<SbFeatureParam> params;

  // If a string parameter is overidden, we must make sure that the string value
  // lives until the end of the InitializeStarboardFeatures() call. These
  // strings will be placed inside this vector to ensure this.
  std::vector<std::string> overridden_strings;

  for (const base::Feature* feature : kStarboardFeatures) {
    features.push_back(
        SbFeature{feature->name, base::FeatureList::IsEnabled(*feature)});
  }

  for (const SbParamWrapper& wrapper : kStarboardParams) {
    SbFeatureParam param{wrapper.feature->name, wrapper.param_name,
                         wrapper.type};

    // Assign the value of the SbFeatureParam based on type.
    switch (wrapper.type) {
      case SbFeatureParamTypeBool:
        param.value.bool_value = wrapper.GetValue<bool>();
        break;
      case SbFeatureParamTypeInt:
        param.value.int_value = wrapper.GetValue<int>();
        break;
      case SbFeatureParamTypeDouble:
        param.value.double_value = wrapper.GetValue<double>();
        break;
      // if GetValue<std::string>() returns a non-empty string, we store that
      // string inside overridden_strings to ensure it stays alive throughout
      // the entire lifetime of InitializeStarboardFeatures().
      case SbFeatureParamTypeString: {
        std::string updated_string = wrapper.GetValue<std::string>();
        if (!updated_string.empty()) {
          overridden_strings.push_back(std::move(updated_string));
          param.value.string_value = overridden_strings.back().c_str();
        } else {
          param.value.string_value =
              (std::get<std::string>(wrapper.default_value)).c_str();
        }
        break;
      }
      case SbFeatureParamTypeTime:
        param.value.time_value =
            wrapper.GetValue<base::TimeDelta>().InMicroseconds();
    }
    params.push_back(param);
  }

  extension_api->InitializeStarboardFeatures(features.data(), features.size(),
                                             params.data(), params.size());
}
}  // namespace cobalt::features
