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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_experiments/experiments_utils.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_long_string.h"

namespace blink {

std::optional<base::Value::Dict> ParseConfigToDictionary(
    const ExperimentConfiguration* experiment_configuration) {
  base::Value::Dict experiment_config_dict;

  base::Value::Dict features;
  if (experiment_configuration->hasFeatures()) {
    for (auto& feature_name_and_value : experiment_configuration->features()) {
      features.Set(feature_name_and_value.first.Utf8(),
                   feature_name_and_value.second);
    }
  }
  experiment_config_dict.Set(cobalt::kExperimentConfigFeatures,
                             std::move(features));

  // All FieldTrialParams are stored as strings, including booleans.
  base::Value::Dict feature_params;
  std::string param_value;
  if (experiment_configuration->hasFeatureParams()) {
    for (auto& param_name_and_value :
         experiment_configuration->featureParams()) {
      if (param_name_and_value.second->IsString()) {
        param_value = param_name_and_value.second->GetAsString().Utf8();
      } else if (param_name_and_value.second->IsLong()) {
        param_value = std::to_string(param_name_and_value.second->GetAsLong());
      } else if (param_name_and_value.second->GetAsBoolean()) {
        param_value = "true";
      } else if (!param_name_and_value.second->GetAsBoolean()) {
        param_value = "false";
      } else {
        return std::nullopt;
      }
      feature_params.Set(param_name_and_value.first.Utf8(), param_value);
    }
  }
  experiment_config_dict.Set(cobalt::kExperimentConfigFeatureParams,
                             std::move(feature_params));

  base::Value::List experiment_ids;
  if (experiment_configuration->hasExperimentIds()) {
    for (int exp_id : experiment_configuration->experimentIds()) {
      experiment_ids.Append(exp_id);
    }
  }
  experiment_config_dict.Set(cobalt::kExperimentConfigExpIds,
                             std::move(experiment_ids));
  return experiment_config_dict;
}

}  // namespace blink
