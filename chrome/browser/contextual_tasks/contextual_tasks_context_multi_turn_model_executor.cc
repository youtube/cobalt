// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_multi_turn_model_executor.h"

#include <algorithm>

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace contextual_tasks {

ContextualTasksContextMultiTurnModelExecutor::
    ContextualTasksContextMultiTurnModelExecutor() = default;
ContextualTasksContextMultiTurnModelExecutor::
    ~ContextualTasksContextMultiTurnModelExecutor() = default;

bool ContextualTasksContextMultiTurnModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::vector<float>& input) {
  if (input_tensors.empty() || !input_tensors[0]) {
    return false;
  }

  return tflite::task::core::PopulateTensor<float>(input, input_tensors[0])
      .ok();
}

std::optional<std::vector<float>>
ContextualTasksContextMultiTurnModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  if (output_tensors.empty() || !output_tensors[0]) {
    return std::nullopt;
  }

  const TfLiteTensor* tensor = output_tensors[0];
  std::vector<float> output;
  if (!tflite::task::core::PopulateVector<float>(tensor, &output).ok() ||
      output.empty()) {
    return std::nullopt;
  }

  return output;
}

}  // namespace contextual_tasks
