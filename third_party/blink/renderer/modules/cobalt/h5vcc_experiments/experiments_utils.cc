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

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_double_long_string.h"

namespace blink {

bool IsTrueDouble(double num) {
  int floored_double = base::ClampFloor<int>(num);
  return num != static_cast<double>(floored_double);
}

std::optional<base::Value::Dict> ParseConfigToDictionary(
    const ExperimentConfiguration* experiment_configuration) {
  base::Value::Dict experiment_config_dict;

  // Reject experiment config if any of the required field is missing.
  if (!experiment_configuration->hasActiveExperimentConfigData() ||
      !experiment_configuration->hasLatestExperimentConfigHashData() ||
      !experiment_configuration->hasFeatures() ||
      !experiment_configuration->hasFeatureParams()) {
    return std::nullopt;
  }

  experiment_config_dict.Set(
      cobalt::kExperimentConfigActiveConfigData,
      experiment_configuration->activeExperimentConfigData().Utf8());

  experiment_config_dict.Set(
      cobalt::kLatestConfigHash,
      experiment_configuration->latestExperimentConfigHashData().Utf8());

  base::Value::Dict features;
  for (auto& feature_name_and_value : experiment_configuration->features()) {
    features.Set(feature_name_and_value.first.Utf8(),
                 feature_name_and_value.second);
  }
  experiment_config_dict.Set(cobalt::kExperimentConfigFeatures,
                             std::move(features));

  // All FieldTrialParams are stored as strings, including booleans.
  base::Value::Dict feature_params;
  std::string param_value;
  for (auto& param_name_and_value : experiment_configuration->featureParams()) {
    if (param_name_and_value.second->IsString()) {
      param_value = param_name_and_value.second->GetAsString().Utf8();
    } else if (param_name_and_value.second->IsLong()) {
      param_value = std::to_string(param_name_and_value.second->GetAsLong());
    } else if (param_name_and_value.second->IsDouble()) {
      double received_double = param_name_and_value.second->GetAsDouble();
      // Record the number as a whole number without decimal place if the
      // underlying value is an int.
      param_value =
          IsTrueDouble(received_double)
              ? base::NumberToString(received_double)
              : base::NumberToString(base::ClampFloor<int>(received_double));
    } else if (param_name_and_value.second->GetAsBoolean()) {
      param_value = "true";
    } else if (!param_name_and_value.second->GetAsBoolean()) {
      param_value = "false";
    } else {
      return std::nullopt;
    }
    feature_params.Set(param_name_and_value.first.Utf8(), param_value);
  }
  experiment_config_dict.Set(cobalt::kExperimentConfigFeatureParams,
                             std::move(feature_params));

  return experiment_config_dict;
}

std::optional<base::Value::Dict> ParseSettingsToDictionary(
    const HeapVector<
        std::pair<WTF::String, Member<V8UnionBooleanOrDoubleOrLongOrString>>>&
        settings) {
  base::Value::Dict settings_dict;

  for (auto& setting_name_and_value : settings) {
    std::string setting_name = setting_name_and_value.first.Utf8();
    if (setting_name_and_value.second->IsString()) {
      std::string param_value =
          setting_name_and_value.second->GetAsString().Utf8();
      settings_dict.Set(setting_name, param_value);
    } else if (setting_name_and_value.second->IsLong()) {
      int param_value = setting_name_and_value.second->GetAsLong();
      settings_dict.Set(setting_name, param_value);
    } else if (setting_name_and_value.second->IsDouble()) {
      double received_double = setting_name_and_value.second->GetAsDouble();
      // Record the number as a whole number without decimal place if the
      // underlying value is an int.
      if (IsTrueDouble(received_double)) {
        settings_dict.Set(setting_name, received_double);
      } else {
        settings_dict.Set(setting_name, base::ClampFloor<int>(received_double));
      }
    } else if (setting_name_and_value.second->IsBoolean()) {
      bool param_value = setting_name_and_value.second->GetAsBoolean();
      settings_dict.Set(setting_name, param_value);
    } else {
      return std::nullopt;
    }
  }
  return settings_dict;
}

}  // namespace blink
