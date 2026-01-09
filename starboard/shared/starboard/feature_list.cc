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

#include <cstring>
#include <mutex>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"

namespace starboard::features {
namespace {
std::string ParamTypeToString(const SbFeatureParamType& type) {
  switch (type) {
    case SbFeatureParamTypeBool:
      return "SbFeatureParamTypeBool";
    case SbFeatureParamTypeInt:
      return "SbFeatureParamTypeInt";
    case SbFeatureParamTypeDouble:
      return "SbFeatureParamTypeDouble";
    case SbFeatureParamTypeString:
      return "SbFeatureParamTypeString";
    case SbFeatureParamTypeTime:
      return "SbFeatureParamTypeTime";
    default:
      SB_LOG(ERROR) << "ParamTypeToString() received an invalid Param type. "
                       "Please ensure that a valid Param is given.";
      return "Invalid Param type given.";
  }
}
}  // namespace

SB_ONCE_INITIALIZE_FUNCTION(FeatureList, FeatureList::GetInstance)

// static
void FeatureList::InitializeFeatureList(const SbFeature* features,
                                        size_t number_of_features,
                                        const SbFeatureParam* params,
                                        size_t number_of_params) {
  FeatureList* instance = GetInstance();
  SB_CHECK(instance)
      << "Starboard FeatureList Instance has not been initialized.";

  std::lock_guard lock(instance->mutex_);

  // Store the updated features inside of our instance's feature map.
  for (size_t i = 0; i < number_of_features; i++) {
    const SbFeature& feature = features[i];
    auto& feature_map = instance->features_;

    SB_CHECK_GT(strlen(feature.name), 0U)
        << "Features are not allowed to have empty strings as names.";
    SB_CHECK((feature_map.find(feature.name)) == (feature_map.end()))
        << "Duplicate Features are not allowed.";
    feature_map[feature.name] = feature.is_enabled;
  }

  // Store the updated params inside of our instance's param map.
  for (size_t i = 0; i < number_of_params; i++) {
    SbFeatureParam param = params[i];

    SB_CHECK_GT(strlen(param.feature_name), 0U)
        << "A parameter cannot belong to a feature whose name is an empty "
           "string.";
    SB_CHECK_GT(strlen(param.param_name), 0U)
        << "Parameters are not allowed to have empty strings as names.";
    ParamValue value;
    switch (param.type) {
      case SbFeatureParamTypeBool:
        value = param.value.bool_value;
        break;
      case SbFeatureParamTypeInt:
        value = param.value.int_value;
        break;
      case SbFeatureParamTypeDouble:
        value = param.value.double_value;
        break;
      case SbFeatureParamTypeString:
        value = std::string(param.value.string_value);
        break;
      case SbFeatureParamTypeTime:
        value = param.value.time_value;
        break;
      default:
        SB_LOG(ERROR)
            << "Parameter " << param.param_name
            << " has an invalid type. This shouldn't happen. Check that the "
               "params from starboard_features_initialization.cc are passed "
               "down correctly. Any calls from this parameter will crash the "
               "app.";
        value = 0;
    }

    auto& feature_param_map = instance->params_;
    SB_CHECK(feature_param_map.find(param.feature_name) ==
                 feature_param_map.end() ||
             feature_param_map[param.feature_name].find(param.param_name) ==
                 feature_param_map[param.feature_name].end())
        << "All parameters that a feature owns must have unique names.";

    feature_param_map[param.feature_name][param.param_name] =
        std::make_pair(param.type, value);
  }
  instance->is_initialized_ = true;
}

void FeatureList::ValidateParam(const std::string& feature_name,
                                const std::string& param_name,
                                const SbFeatureParamType& param_type) {
  SB_CHECK(IsInitialized())
      << "Starboard features and parameters are not initialized.";

  const auto& feature_it = params_.find(feature_name);
  SB_CHECK(feature_it != params_.end())
      << "Feature " << feature_name
      << " is not initialized in the Starboard level. Is the feature "
         "initialized in starboard/extension/feature_config.h?";

  const auto& param_map = feature_it->second;
  const auto& param_it = param_map.find(param_name);
  SB_CHECK(param_it != param_map.end())
      << "Parameter " << param_name
      << " is not initialized in the Starboard level. Is the parameter "
         "initialized in starboard/extension/feature_config.h?";

  const auto& [param_entry_type, _] = param_it->second;
  SB_CHECK_EQ(param_entry_type, param_type)
      << "The type of parameter " << param_name
      << " does not match its declared type in "
         "starboard/extension/feature_config.h."
      << " It should have type" << ParamTypeToString(param_type)
      << " but has type " << ParamTypeToString(param_entry_type);
}

// static
bool FeatureList::IsEnabled(const SbFeature& feature) {
  return IsEnabledByName(feature.name);
}

// static
bool FeatureList::IsEnabledByName(const std::string& feature_name) {
  FeatureList* instance = GetInstance();
  std::lock_guard lock(instance->mutex_);

  // IsEnabled can only be called after the FeatureList has been initialized.
  SB_CHECK(instance->IsInitialized())
      << "Starboard features and parameters are not initialized.";

  const auto& feature_map = instance->features_;
  auto feature_it = feature_map.find(feature_name);
  SB_CHECK(feature_it != feature_map.end())
      << "Feature " << feature_name
      << " is not initialized in the Starboard level. Is the feature "
         "initialized in starboard/extension/feature_config.h?";
  return feature_it->second;
}

template <>
bool FeatureList::GetParam(const SbFeatureParamExt<bool>& param) {
  FeatureList* instance = GetInstance();
  std::lock_guard lock(instance->mutex_);
  instance->ValidateParam(param.feature_name, param.param_name,
                          SbFeatureParamTypeBool);
  return std::get<bool>(
      instance->params_[param.feature_name][param.param_name].second);
}

template <>
int FeatureList::GetParam(const SbFeatureParamExt<int>& param) {
  FeatureList* instance = GetInstance();
  std::lock_guard lock(instance->mutex_);
  instance->ValidateParam(param.feature_name, param.param_name,
                          SbFeatureParamTypeInt);
  return std::get<int>(
      instance->params_[param.feature_name][param.param_name].second);
}

template <>
double FeatureList::GetParam(const SbFeatureParamExt<double>& param) {
  FeatureList* instance = GetInstance();
  std::lock_guard lock(instance->mutex_);
  instance->ValidateParam(param.feature_name, param.param_name,
                          SbFeatureParamTypeDouble);
  return std::get<double>(
      instance->params_[param.feature_name][param.param_name].second);
}

template <>
std::string FeatureList::GetParam(const SbFeatureParamExt<std::string>& param) {
  FeatureList* instance = GetInstance();
  std::lock_guard lock(instance->mutex_);
  instance->ValidateParam(param.feature_name, param.param_name,
                          SbFeatureParamTypeString);
  return std::get<std::string>(
      instance->params_[param.feature_name][param.param_name].second);
}

template <>
int64_t FeatureList::GetParam(const SbFeatureParamExt<int64_t>& param) {
  FeatureList* instance = GetInstance();
  std::lock_guard lock(instance->mutex_);
  instance->ValidateParam(param.feature_name, param.param_name,
                          SbFeatureParamTypeTime);
  return std::get<int64_t>(
      instance->params_[param.feature_name][param.param_name].second);
}
}  // namespace starboard::features
