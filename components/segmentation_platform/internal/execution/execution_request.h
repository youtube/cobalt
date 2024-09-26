// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_EXECUTION_REQUEST_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_EXECUTION_REQUEST_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}

class ModelProvider;

struct ModelExecutionResult {
  ModelExecutionResult(const ModelProvider::Request& inputs,
                       const ModelProvider::Response& scores);
  explicit ModelExecutionResult(ModelExecutionStatus status);
  ~ModelExecutionResult();

  ModelExecutionResult(const ModelExecutionResult&) = delete;
  ModelExecutionResult& operator=(const ModelExecutionResult&) = delete;

  // The float value is only valid when ModelExecutionStatus == kSuccess.
  // TODO(ritikagup): Change ModelProvider::Response as key value pair in
  // future.
  ModelProvider::Response scores;
  ModelExecutionStatus status;

  ModelProvider::Request inputs;
};

// Request for model execution.
struct ExecutionRequest {
  using ModelExecutionCallback =
      base::OnceCallback<void(std::unique_ptr<ModelExecutionResult>)>;

  ExecutionRequest();
  ~ExecutionRequest();

  // Required: The segment info to use for model execution.
  raw_ptr<const proto::SegmentInfo> segment_info = nullptr;

  // The model provider used to execute the model.
  raw_ptr<ModelProvider> model_provider = nullptr;

  // Current context of the browser that is needed by feature processor for some
  // of the models.
  scoped_refptr<InputContext> input_context;

  // Save result of execution to the database.
  bool save_result_to_db = false;

  // Record metrics for default model instead of optimization_guide based
  // models.
  bool record_metrics_for_default = false;

  // returns result as by callback, to be used when `save_result_to_db` is
  // false.
  ModelExecutionCallback callback;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_EXECUTION_REQUEST_H_
