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

#include "starboard/shared/starboard/feature_list.h"

#include "starboard/common/log.h"
#include "starboard/common/once.h"

namespace starboard::features {

SB_ONCE_INITIALIZE_FUNCTION(FeatureList, FeatureList::GetInstance)

// static
void FeatureList::InitializeFeatureList(const SbFeature* features,
                                        size_t number_of_features,
                                        const SbFeatureParam* params,
                                        size_t number_of_params) {
  FeatureList* instance = GetInstance();
  ScopedLock lock(instance->mutex_);

  // Store the updated features inside of our instance's feature map.
  for (size_t i = 0; i < number_of_features; i++) {
    (instance->features_)[features[i].name] = features[i].is_enabled;
  }

  // Store the updated params inside of our instance's param map.
  for (size_t i = 0; i < number_of_params; i++) {
    ParamValue value;
    switch (params[i].type) {
      case SbFeatureParamTypeBool:
        value = params[i].value.bool_value;
        break;
      case SbFeatureParamTypeInt:
        value = params[i].value.int_value;
        break;
      case SbFeatureParamTypeDouble:
        value = params[i].value.double_value;
        break;
      case SbFeatureParamTypeString:
        value = std::string(params[i].value.string_value);
        break;
      case SbFeatureParamTypeTime:
        value = params[i].value.time_value;
        break;
    }

    (instance->params_)[params[i].feature_name][params[i].param_name] =
        std::make_pair(params[i].type, value);
  }
  instance->is_initialized_ = true;
}

bool FeatureList::checkFeatureAndParamExistence(const std::string& feature_name,
                                                const std::string& param_name) {
  if (!IsInitialized()) {
    return false;
  }
  auto feature_entry = params_.find(feature_name);
  if (feature_entry == params_.end()) {
    return false;
  }
  return feature_entry->second.find(param_name) != feature_entry->second.end();
}

// static
bool FeatureList::IsEnabled(const SbFeature& feature) {
  FeatureList* instance = GetInstance();
  ScopedLock lock(instance->mutex_);

  // If IsEnabled is called before the FeatureList is initialized, crash.
  SB_CHECK(instance->IsInitialized());

  // If the feature does not exist in the map, crash.
  SB_CHECK(instance->features_.end() != instance->features_.find(feature.name));
  return instance->features_[feature.name];
}

template <>
bool FeatureList::GetParam(const SbFeatureParamExt<bool>& param) {
  FeatureList* instance = GetInstance();
  ScopedLock lock(instance->mutex_);
  SB_CHECK(instance->checkFeatureAndParamExistence(param.feature_name,
                                                   param.param_name));
  SB_CHECK(instance->params_[param.feature_name][param.param_name].first ==
           SbFeatureParamTypeBool);
  return std::get<bool>(
      instance->params_[param.feature_name][param.param_name].second);
}

template <>
int FeatureList::GetParam(const SbFeatureParamExt<int>& param) {
  FeatureList* instance = GetInstance();
  ScopedLock lock(instance->mutex_);
  SB_CHECK(instance->checkFeatureAndParamExistence(param.feature_name,
                                                   param.param_name));
  SB_CHECK(instance->params_[param.feature_name][param.param_name].first ==
           SbFeatureParamTypeInt);
  return std::get<int>(
      instance->params_[param.feature_name][param.param_name].second);
}

template <>
double FeatureList::GetParam(const SbFeatureParamExt<double>& param) {
  FeatureList* instance = GetInstance();
  ScopedLock lock(instance->mutex_);
  SB_CHECK(instance->checkFeatureAndParamExistence(param.feature_name,
                                                   param.param_name));
  SB_CHECK(instance->params_[param.feature_name][param.param_name].first ==
           SbFeatureParamTypeDouble);
  return std::get<double>(
      instance->params_[param.feature_name][param.param_name].second);
}

template <>
std::string FeatureList::GetParam(const SbFeatureParamExt<std::string>& param) {
  FeatureList* instance = GetInstance();
  ScopedLock lock(instance->mutex_);
  SB_CHECK(instance->checkFeatureAndParamExistence(param.feature_name,
                                                   param.param_name));
  SB_CHECK(instance->params_[param.feature_name][param.param_name].first ==
           SbFeatureParamTypeString);
  return std::get<std::string>(
      instance->params_[param.feature_name][param.param_name].second);
}

template <>
int64_t FeatureList::GetParam(const SbFeatureParamExt<int64_t>& param) {
  FeatureList* instance = GetInstance();
  ScopedLock lock(instance->mutex_);
  SB_CHECK(instance->checkFeatureAndParamExistence(param.feature_name,
                                                   param.param_name));
  SB_CHECK(instance->params_[param.feature_name][param.param_name].first ==
           SbFeatureParamTypeTime);
  return std::get<int64_t>(
      instance->params_[param.feature_name][param.param_name].second);
}
}  // namespace starboard::features
