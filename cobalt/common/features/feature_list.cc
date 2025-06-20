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

#include "cobalt/common/features/features.h"

#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "starboard/extension/features.h"
#include "starboard/system.h"

namespace cobalt::features {
namespace {

struct SbParamWrapper {
  const base::Feature* feature;
  const char* feature_name;
  const char* param_name;
  SbFeatureParamType type;
  union {
    bool bool_value;
    int int_value;
    size_t size_value;
    double double_value;
    const char* string_value;
    int64_t time_value;
  };

  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* feature_name,
                           const char* name,
                           bool value)
      : feature(feature),
        feature_name(feature_name),
        param_name(name),
        type(SbFeatureParamTypeBool),
        bool_value(value) {}

  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* feature_name,
                           const char* name,
                           int value)
      : feature(feature),
        feature_name(feature_name),
        param_name(name),
        type(SbFeatureParamTypeInt),
        int_value(value) {}

  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* feature_name,
                           const char* name,
                           size_t value)
      : feature(feature),
        feature_name(feature_name),
        param_name(name),
        type(SbFeatureParamTypeSize),
        size_value(value) {}

  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* feature_name,
                           const char* name,
                           double value)
      : feature(feature),
        feature_name(feature_name),
        param_name(name),
        type(SbFeatureParamTypeDouble),
        double_value(value) {}

  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* feature_name,
                           const char* name,
                           const char* value)
      : feature(feature),
        feature_name(feature_name),
        param_name(name),
        type(SbFeatureParamTypeString),
        string_value(value) {}

  constexpr SbParamWrapper(const base::Feature* feature,
                           const char* feature_name,
                           const char* name,
                           int64_t value)
      : feature(feature),
        feature_name(feature_name),
        param_name(name),
        type(SbFeatureParamTypeTime),
        time_value(value) {}
};

void parseFeatureParam(SbFeatureParam& sbparam,
                       const SbParamWrapper& wrapper_param,
                       std::string param_value) {
  switch (wrapper_param.type) {
    case (SbFeatureParamTypeBool):
      if (param_value == "true") {
        sbparam.bool_value = true;
      } else if (param_value == "false") {
        sbparam.bool_value = false;
      } else {
        sbparam.bool_value = wrapper_param.bool_value;
      }
      break;
    case (SbFeatureParamTypeInt):
      if (!base::StringToInt(param_value, &sbparam.int_value)) {
        sbparam.int_value = wrapper_param.int_value;
      }
      break;
    case (SbFeatureParamTypeSize):
      if (!base::StringToSizeT(param_value, &sbparam.size_value)) {
        sbparam.size_value = wrapper_param.size_value;
      }
      break;
    case (SbFeatureParamTypeDouble):
      if (!base::StringToDouble(param_value, &sbparam.double_value)) {
        sbparam.double_value = wrapper_param.double_value;
      }
      break;
    case (SbFeatureParamTypeString):
      sbparam.string_value =
          param_value == "" ? wrapper_param.string_value : param_value.c_str();
      break;
    case (SbFeatureParamTypeTime):
      // TODO: Maybe use uint64 instead?
      if (!base::StringToInt64(param_value, &sbparam.time_value)) {
        sbparam.time_value = wrapper_param.time_value;
      }
  }
}
}  // namespace

#define FEATURE_LIST_START                                     \
  constexpr const std::pair<const base::Feature*, const char*> \
      kStarboardFeatures[] = {
#define FEATURE_LIST_END \
  }                      \
  ;
#define FEATURE_PARAM_LIST_START const SbParamWrapper kStarboardParams[] = {
#define FEATURE_PARAM_LIST_END \
  }                            \
  ;

#define STARBOARD_FEATURE(feature, name, default_state) {&feature, name},

#define STARBOARD_FEATURE_PARAM(T, param_object_name, feature_object_name,   \
                                param_name, default_value)                   \
  SbParamWrapper(&feature_object_name, feature_object_name.name, param_name, \
                 static_cast<T>(default_value)),

#include "starboard/common/feature_config.h"

#undef STARBOARD_FEATURE
#undef STARBOARD_FEATURE_PARAM
#undef FEATURE_LIST_START
#undef FEATURE_LIST_END
#undef FEATURE_PARAM_LIST_START
#undef FEATURE_PARAM_LIST_END

#define ARRAY_SIZE(arr) ((sizeof(arr) / sizeof(arr[0])))
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

  size_t number_of_features = ARRAY_SIZE(kStarboardFeatures);
  size_t number_of_params = ARRAY_SIZE(kStarboardParams);
  std::vector<SbFeature> features;
  std::vector<SbFeatureParam> params;

  for (const std::pair<const base::Feature*, const char*>& feature :
       kStarboardFeatures) {
    absl::optional<bool> overwritten_state =
        base::FeatureList::GetStateIfOverridden(*(feature.first));

    if (!overwritten_state.has_value()) {
      features.push_back(SbFeature{
          feature.second, base::FeatureList::IsEnabled(*(feature.first))});
    } else {
      features.push_back(
          SbFeature{feature.second, static_cast<bool>(overwritten_state)});
    }
  }

  for (const SbParamWrapper& param : kStarboardParams) {
    std::string current_value = base::GetFieldTrialParamValueByFeature(
        *(param.feature), param.param_name);
    SbFeatureParam SbParam = {param.feature_name, param.param_name, param.type};
    parseFeatureParam(SbParam, param, current_value);
    params.push_back(SbParam);
  }

  extension_api->InitializeStarboardFeatures(features.data(), features.size(),
                                             params.data(), params.size());
}
}  // namespace cobalt::features
