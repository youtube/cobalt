// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor_impl.h"

#include <cmath>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/optimization_guide/core/model_quality/model_execution_logging_wrappers.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/user_annotations/user_annotations_service.h"
#include "components/user_annotations/user_annotations_types.h"

namespace autofill_ai {

AutofillAiModelExecutorImpl::AutofillAiModelExecutorImpl(
    optimization_guide::OptimizationGuideModelExecutor* model_executor,
    optimization_guide::ModelQualityLogsUploaderService* logs_uploader,
    user_annotations::UserAnnotationsService* user_annotations_service)
    : model_executor_(model_executor),
      user_annotations_service_(user_annotations_service) {
  CHECK(model_executor_);
  CHECK(user_annotations_service_);
  if (logs_uploader) {
    logs_uploader_ = logs_uploader->GetWeakPtr();
  }
}
AutofillAiModelExecutorImpl::~AutofillAiModelExecutorImpl() = default;

void AutofillAiModelExecutorImpl::GetPredictions(
    autofill::FormData form_data,
    base::flat_map<autofill::FieldGlobalId, bool> field_eligibility_map,
    base::flat_map<autofill::FieldGlobalId, bool> field_sensitivity_map,
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    PredictionsReceivedCallback callback) {
  user_annotations_service_->RetrieveAllEntries(base::BindOnce(
      &AutofillAiModelExecutorImpl::OnUserAnnotationsRetrieved,
      weak_ptr_factory_.GetWeakPtr(), std::move(form_data),
      std::move(field_eligibility_map), std::move(field_sensitivity_map),
      std::move(ax_tree_update), std::move(callback)));
}

void AutofillAiModelExecutorImpl::OnUserAnnotationsRetrieved(
    autofill::FormData form_data,
    const base::flat_map<autofill::FieldGlobalId, bool>& field_eligibility_map,
    const base::flat_map<autofill::FieldGlobalId, bool>& field_sensitivity_map,
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    PredictionsReceivedCallback callback,
    user_annotations::UserAnnotationsEntries user_annotations) {
  // At this point there should be user annotations. Return an error if there
  // aren't.
  // TODO(crbug.com/361414075): Check that `user_annotations` aren't empty in
  // `AutofillAiDelegate::ShouldProvidePredictionImprovements()`.
  if (user_annotations.empty()) {
    std::move(callback).Run(base::unexpected(false), std::nullopt);
    return;
  }

  // Construct request.
  optimization_guide::proto::FormsPredictionsRequest request;
  optimization_guide::proto::PageContext* page_context =
      request.mutable_page_context();
  if (kSendTitleURL.Get()) {
    page_context->set_url(form_data.url().spec());
    page_context->set_title(ax_tree_update.tree_data().title());
  } else {
    page_context->set_url(form_data.main_frame_origin().Serialize());
  }
  *page_context->mutable_ax_tree_data() = std::move(ax_tree_update);

  *request.mutable_form_data() =
      ToFormDataProto(form_data, field_eligibility_map, field_sensitivity_map);
  *request.mutable_entries() = {
      std::make_move_iterator(user_annotations.begin()),
      std::make_move_iterator(user_annotations.end())};

  SetLatestRequestForDebugging(request);
  optimization_guide::ModelExecutionCallbackWithLogging<
      optimization_guide::proto::FormsPredictionsLoggingData>
      wrapper_callback =
          base::BindOnce(&AutofillAiModelExecutorImpl::OnModelExecuted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(form_data),
                         std::move(callback));
  optimization_guide::ExecuteModelWithLogging(
      model_executor_,
      optimization_guide::ModelBasedCapabilityKey::kFormsPredictions, request,
      kExecutionTimeout.Get(), std::move(wrapper_callback));
}

void AutofillAiModelExecutorImpl::OnModelExecuted(
    autofill::FormData form_data,
    PredictionsReceivedCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult execution_result,
    std::unique_ptr<optimization_guide::proto::FormsPredictionsLoggingData>
        logging_data) {
  CHECK(logging_data);
  auto log_entry = std::make_unique<optimization_guide::ModelQualityLogEntry>(
      logs_uploader_);
  const std::optional<std::string> execution_id =
      logging_data->model_execution_info().execution_id();
  if (!execution_result.response.has_value()) {
    std::move(callback).Run(base::unexpected(false), execution_id);
    return;
  }

  SetLatestResponseForDebugging(
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::FormsPredictionsResponse>(
          execution_result.response.value()));

  if (!GetLatestResponse()) {
    std::move(callback).Run(base::unexpected(false), execution_id);
    return;
  }

  std::move(callback).Run(
      ExtractPredictions(form_data, GetLatestResponse()->form_data()),
      execution_id);
}

// static
AutofillAiModelExecutor::PredictionsByGlobalId
AutofillAiModelExecutorImpl::ExtractPredictions(
    const autofill::FormData& form_data,
    const optimization_guide::proto::FilledFormData& form_data_proto) {
  std::vector<std::pair<autofill::FieldGlobalId, Prediction>> predictions;
  const std::vector<autofill::FormFieldData>& fields = form_data.fields();
  for (const optimization_guide::proto::FilledFormFieldData&
           filled_form_field_proto : form_data_proto.filled_form_field_data()) {
    // Only the first predicted value is used at the moment.
    if (filled_form_field_proto.predicted_values_size() == 0 ||
        filled_form_field_proto.predicted_values()[0].value().empty()) {
      continue;
    }

    size_t request_field_index =
        static_cast<size_t>(filled_form_field_proto.request_field_index());
    if (request_field_index >= fields.size()) {
      // Execution returned an out-of-bounds field index.
      continue;
    }

    const autofill::FormFieldData& field = fields.at(request_field_index);
    if (base::UTF8ToUTF16(filled_form_field_proto.field_data().field_label()) !=
        field.label()) {
      // Skip over if the label is no longer the same and the execution provided
      // a wrong index.
      continue;
    }

    std::u16string predicted_value = base::UTF8ToUTF16(
        filled_form_field_proto.predicted_values()[0].value());
    std::u16string label =
        filled_form_field_proto.normalized_label().empty()
            ? (field.label().empty() ? field.placeholder() : field.label())
            : base::UTF8ToUTF16(filled_form_field_proto.normalized_label());

    if (field.IsSelectElement()) {
      // Reject the prediction if it equals the currently selected option.
      if (field.selected_option().has_value() &&
          field.selected_option()->text == predicted_value) {
        continue;
      }

      // Ensure that the predicted value actually is one of the select
      // options.
      auto predicted_select_option_it = std::ranges::find(
          field.options(), predicted_value, &autofill::SelectOption::text);
      if (predicted_select_option_it == field.options().end()) {
        continue;
      }

      predictions.emplace_back(
          field.global_id(),
          Prediction{predicted_select_option_it->value, std::move(label),
                     field.IsFocusable(), predicted_select_option_it->text});
      continue;
    }
    // Skip predictions for non-empty text fields.
    else if (field.IsTextInputElement() && !field.value().empty()) {
      continue;
    }

    predictions.emplace_back(field.global_id(),
                             Prediction{std::move(predicted_value),
                                        std::move(label), field.IsFocusable()});
  }
  return predictions;
}

void AutofillAiModelExecutorImpl::SetLatestRequestForDebugging(
    optimization_guide::proto::FormsPredictionsRequest request) {
  // Reset `latest_response_` to ensure it always matches `latest_request_`, if
  // it exists.
  latest_response_.reset();
  latest_request_ = std::move(request);
}

void AutofillAiModelExecutorImpl::SetLatestResponseForDebugging(
    std::optional<optimization_guide::proto::FormsPredictionsResponse>
        response) {
  latest_response_ = std::move(response);
}

const std::optional<optimization_guide::proto::FormsPredictionsRequest>&
AutofillAiModelExecutorImpl::GetLatestRequest() const {
  return latest_request_;
}

const std::optional<optimization_guide::proto::FormsPredictionsResponse>&
AutofillAiModelExecutorImpl::GetLatestResponse() const {
  return latest_response_;
}

}  // namespace autofill_ai
