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

SB_ONCE_INITIALIZE_FUNCTION(FeatureList, FeatureList::GetInstance);

// static
void FeatureList::InitializeFeatureList(const SbFeature* features,
                                        size_t number_of_features,
                                        const SbFeatureParam* params,
                                        size_t number_of_params) {
  FeatureList* feature_list_instance = FeatureList::GetInstance();
  ScopedLock lock(feature_list_instance->mutex_);

  // Store the updated features inside of our instance's feature map
  for (int i = 0; i < number_of_features; i++) {
    (feature_list_instance->features_)[features[i].name] =
        features[i].is_enabled;
  }

  // Store the updated params inside of our instance's param map
  for (int i = 0; i < number_of_params; i++) {
    switch (params[i].type) {
      case SbFeatureParamTypeBool:
        (feature_list_instance
             ->params_)[params[i].feature_name][params[i].param_name] =
            params[i].bool_value;
        break;
      case SbFeatureParamTypeInt:
        (feature_list_instance
             ->params_)[params[i].feature_name][params[i].param_name] =
            params[i].int_value;
        break;
      case SbFeatureParamTypeSize:
        (feature_list_instance
             ->params_)[params[i].feature_name][params[i].param_name] =
            params[i].size_value;
        break;
      case SbFeatureParamTypeDouble:
        (feature_list_instance
             ->params_)[params[i].feature_name][params[i].param_name] =
            params[i].double_value;
        break;
      case SbFeatureParamTypeString:
        (feature_list_instance
             ->params_)[params[i].feature_name][params[i].param_name] =
            std::string(params[i].string_value);
        break;
      case SbFeatureParamTypeTime:
        (feature_list_instance
             ->params_)[params[i].feature_name][params[i].param_name] =
            params[i].time_value;
    }
  }

  feature_list_instance->IS_INITIALIZED_ = true;
}

// static
bool FeatureList::IsEnabled(const SbFeature& feature) {
  FeatureList* feature_list_instance = FeatureList::GetInstance();
  ScopedLock lock(feature_list_instance->mutex_);

  // If IsEnabled is called before the FeatureList is initialized, we
  // crash on a devel build. In production, we will return false always.
  SB_DCHECK(feature_list_instance->IS_INITIALIZED_);
  if (!feature_list_instance->IS_INITIALIZED_) {
    SB_LOG(ERROR)
        << feature.name
        << " is being accessed without the FeatureList being initialized.";
    return false;
  }
  return feature_list_instance->features_[feature.name];
}

// If any of the GetParam functions are called before the FeatureList is
// initialized, the following will happen depending on build:
//
// devel: crash
//
// production (qa/gold): return a pre-determined value. This value is dependent
// on which version of GetParam is called. These types and their return values
// are: int, double, size_t, int64_t: 0 std::string: "" (the empty string) bool:
// false

template <>
bool FeatureList::GetParam(const SbFeatureParamExt<bool>& param) {
  FeatureList* feature_list_instance = FeatureList::GetInstance();
  ScopedLock lock(feature_list_instance->mutex_);
  SB_DCHECK(feature_list_instance->IS_INITIALIZED_);
  if (!feature_list_instance->IS_INITIALIZED_) {
    return false;
  }
  return std::get<bool>(
      feature_list_instance->params_[param.feature_name][param.param_name]);
}

template <>
int FeatureList::GetParam(const SbFeatureParamExt<int>& param) {
  FeatureList* feature_list_instance = FeatureList::GetInstance();
  ScopedLock lock(feature_list_instance->mutex_);
  SB_DCHECK(feature_list_instance->IS_INITIALIZED_);
  if (!feature_list_instance->IS_INITIALIZED_) {
    return 0;
  }
  return std::get<int>(
      feature_list_instance->params_[param.feature_name][param.param_name]);
}

template <>
std::string FeatureList::GetParam(const SbFeatureParamExt<std::string>& param) {
  FeatureList* feature_list_instance = FeatureList::GetInstance();
  ScopedLock lock(feature_list_instance->mutex_);
  SB_DCHECK(feature_list_instance->IS_INITIALIZED_);
  if (!feature_list_instance->IS_INITIALIZED_) {
    return "";
  }
  return std::get<std::string>(
      feature_list_instance->params_[param.feature_name][param.param_name]);
}

template <>
size_t FeatureList::GetParam(const SbFeatureParamExt<size_t>& param) {
  FeatureList* feature_list_instance = FeatureList::GetInstance();
  ScopedLock lock(feature_list_instance->mutex_);
  SB_DCHECK(feature_list_instance->IS_INITIALIZED_);
  if (!feature_list_instance->IS_INITIALIZED_) {
    return 0;
  }
  return std::get<size_t>(
      feature_list_instance->params_[param.feature_name][param.param_name]);
}

template <>
int64_t FeatureList::GetParam(const SbFeatureParamExt<int64_t>& param) {
  FeatureList* feature_list_instance = FeatureList::GetInstance();
  ScopedLock lock(feature_list_instance->mutex_);
  SB_DCHECK(feature_list_instance->IS_INITIALIZED_);
  if (!feature_list_instance->IS_INITIALIZED_) {
    return 0;
  }
  return std::get<int64_t>(
      feature_list_instance->params_[param.feature_name][param.param_name]);
}

template <>
double FeatureList::GetParam(const SbFeatureParamExt<double>& param) {
  FeatureList* feature_list_instance = FeatureList::GetInstance();
  ScopedLock lock(feature_list_instance->mutex_);
  SB_DCHECK(feature_list_instance->IS_INITIALIZED_);
  if (!feature_list_instance->IS_INITIALIZED_) {
    return 0;
  }
  return std::get<double>(
      feature_list_instance->params_[param.feature_name][param.param_name]);
}

}  // namespace starboard::features
