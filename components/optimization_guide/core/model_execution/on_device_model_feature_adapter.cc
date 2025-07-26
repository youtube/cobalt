// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"

#include <optional>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/redactor.h"
#include "components/optimization_guide/core/model_execution/response_parser.h"
#include "components/optimization_guide/core/model_execution/response_parser_registry.h"
#include "components/optimization_guide/core/model_execution/simple_response_parser.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"

namespace optimization_guide {

OnDeviceModelFeatureAdapter::OnDeviceModelFeatureAdapter(
    proto::OnDeviceModelExecutionFeatureConfig&& config)
    : config_(config),
      redactor_(Redactor::FromProto(config.output_config().redact_rules())),
      response_streaming_mode_(
          config.output_config().response_streaming_mode()),
      parser_(
          ResponseParserRegistry::Get().CreateParser(config_.output_config())) {
  // Set limits values in `token_limits_`.
  auto& input_config = config_.input_config();
  auto& output_config = config_.output_config();
  token_limits_.max_tokens = features::GetOnDeviceModelMaxTokens();
  token_limits_.min_context_tokens =
      input_config.has_min_context_tokens()
          ? input_config.min_context_tokens()
          : static_cast<uint32_t>(
                features::GetOnDeviceModelMinTokensForContext());
  token_limits_.max_context_tokens =
      input_config.has_max_context_tokens()
          ? input_config.max_context_tokens()
          : static_cast<uint32_t>(
                features::GetOnDeviceModelMaxTokensForContext());
  token_limits_.max_execute_tokens =
      input_config.has_max_execute_tokens()
          ? input_config.max_execute_tokens()
          : static_cast<uint32_t>(
                features::GetOnDeviceModelMaxTokensForExecute());
  token_limits_.max_output_tokens =
      output_config.has_max_output_tokens()
          ? output_config.max_output_tokens()
          : static_cast<uint32_t>(
                features::GetOnDeviceModelMaxTokensForOutput());
}

OnDeviceModelFeatureAdapter::~OnDeviceModelFeatureAdapter() = default;

std::string OnDeviceModelFeatureAdapter::GetStringToCheckForRedacting(
    MultimodalMessageReadView message) const {
  for (const auto& proto_field :
       config_.output_config().redact_rules().fields_to_check()) {
    std::optional<proto::Value> value = message.GetValue(proto_field);
    if (value) {
      const std::string string_value = GetStringFromValue(*value);
      if (!string_value.empty()) {
        return string_value;
      }
    }
  }
  return std::string();
}

std::optional<SubstitutionResult>
OnDeviceModelFeatureAdapter::ConstructInputString(
    MultimodalMessageReadView request,
    bool want_input_context) const {
  if (!config_.has_input_config()) {
    return std::nullopt;
  }
  const auto input_config = config_.input_config();
  if (input_config.request_base_name() != request.GetTypeName()) {
    return std::nullopt;
  }
  return CreateSubstitutions(
      request, want_input_context ? input_config.input_context_substitutions()
                                  : input_config.execute_substitutions());
}

RedactResult OnDeviceModelFeatureAdapter::Redact(
    MultimodalMessageReadView last_message,
    std::string& current_response) const {
  auto redact_string_input = GetStringToCheckForRedacting(last_message);
  base::ElapsedTimer elapsed_timer;
  auto redact_result = redactor_.Redact(redact_string_input, current_response);
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({"OptimizationGuide.ModelExecution.TimeToProcessRedactions.",
                    GetStringNameForModelExecutionFeature(config_.feature())}),
      elapsed_timer.Elapsed());
  return redact_result;
}

bool OnDeviceModelFeatureAdapter::ShouldParseResponse(
    ResponseCompleteness completeness) const {
  return completeness == ResponseCompleteness::kComplete ||
         !parser_->SuppressParsingIncompleteResponse();
}

void OnDeviceModelFeatureAdapter::ParseResponse(
    const MultimodalMessage& request,
    const std::string& model_response,
    size_t previous_response_pos,
    ResponseParser::ResultCallback callback) const {
  std::string redacted_response = model_response;
  auto redact_result = Redact(request.read(), redacted_response);
  if (redact_result != RedactResult::kContinue) {
    std::move(callback).Run(
        base::unexpected(ResponseParsingError::kRejectedPii));
    return;
  }
  if (!parser_) {
    std::move(callback).Run(base::unexpected(ResponseParsingError::kFailed));
    return;
  }

  switch (response_streaming_mode_) {
    case proto::ResponseStreamingMode::STREAMING_MODE_CURRENT_RESPONSE: {
      parser_->ParseAsync(redacted_response, std::move(callback));
      break;
    }

    case proto::ResponseStreamingMode::STREAMING_MODE_CHUNK_BY_CHUNK: {
      // The `redacted_response` is actually not redacted here because the
      // redactor config and chunk-by-chunk mode are mutual exclusive.
      parser_->ParseAsync(redacted_response.substr(previous_response_pos),
                          std::move(callback));
      break;
    }
  }
}

std::optional<proto::TextSafetyRequest>
OnDeviceModelFeatureAdapter::ConstructTextSafetyRequest(
    MultimodalMessageReadView request,
    const std::string& text) const {
  if (!config_.has_text_safety_fallback_config()) {
    return std::nullopt;
  }

  auto& text_safety_fallback_config = config_.text_safety_fallback_config();

  proto::TextSafetyRequest text_safety_request;
  text_safety_request.set_text(text);

  if (text_safety_fallback_config.has_input_url_proto_field()) {
    std::optional<proto::Value> input_url_value =
        request.GetValue(text_safety_fallback_config.input_url_proto_field());
    if (input_url_value) {
      const std::string string_value = GetStringFromValue(*input_url_value);
      text_safety_request.set_url(string_value);
    } else {
      return std::nullopt;
    }
  }

  return text_safety_request;
}

SamplingParamsConfig OnDeviceModelFeatureAdapter::GetSamplingParamsConfig()
    const {
  if (!config_.has_sampling_params()) {
    // Returns default value if the sampling params are not configured.
    return SamplingParamsConfig{
        .default_top_k = uint32_t(features::GetOnDeviceModelDefaultTopK()),
        .default_temperature =
            float(features::GetOnDeviceModelDefaultTemperature()),
    };
  }

  return SamplingParamsConfig{
      .default_top_k = config_.sampling_params().top_k(),
      .default_temperature = config_.sampling_params().temperature(),
  };
}

const proto::Any& OnDeviceModelFeatureAdapter::GetFeatureMetadata() const {
  return config_.feature_metadata();
}

const TokenLimits& OnDeviceModelFeatureAdapter::GetTokenLimits() const {
  return token_limits_;
}

}  // namespace optimization_guide
