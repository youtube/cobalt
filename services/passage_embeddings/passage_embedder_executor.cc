// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder_executor.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "services/passage_embeddings/passage_embedder_execution_task.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/signature_runner.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/tflite_engine.h"

namespace passage_embeddings {

// HistoryModelExecutor

HistoryModelExecutor::HistoryModelExecutor(
    uint32_t input_window_size,
    int eos_id,
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine)
    : task_(std::make_unique<PassageEmbedderExecutionTask>(
          std::move(tflite_engine))),
      input_window_size_(input_window_size),
      eos_id_(eos_id) {}
HistoryModelExecutor::~HistoryModelExecutor() = default;

std::optional<EmbedderExecutionResult> HistoryModelExecutor::Execute(
    const std::vector<int>& raw_tokens) {
  auto output =
      task_->Execute(FormatInput(raw_tokens, input_window_size_, eos_id_));
  if (!output) {
    return std::nullopt;
  }
  return EmbedderExecutionResult{std::move(*output), input_window_size_};
}

std::vector<int> HistoryModelExecutor::FormatInput(
    const std::vector<int>& input,
    uint32_t input_window_size,
    int eos_id) {
  std::vector<int> formatted;
  formatted.reserve(input_window_size);
  const size_t content_to_copy =
      std::min(input.size(), static_cast<size_t>(input_window_size));
  formatted.insert(formatted.end(), input.begin(),
                   input.begin() + content_to_copy);
  if (formatted.size() < input_window_size) {
    formatted.push_back(eos_id);
  }
  // Note: The History Embeddings model expects 0-padding after the EOS
  // marker.
  formatted.resize(input_window_size, 0);
  return formatted;
}

// GemmaModelExecutor

// static
std::vector<GemmaModelExecutor::ModelSignature>
GemmaModelExecutor::ExtractSignatures(
    tflite::task::core::TfLiteEngine* tflite_engine) {
  tflite::Interpreter* interpreter = tflite_engine->interpreter();
  std::vector<ModelSignature> available_signatures;
  for (const std::string* key : interpreter->signature_keys()) {
    CHECK(!key->empty());
    tflite::SignatureRunner* runner =
        interpreter->GetSignatureRunner(key->c_str());

    CHECK(!runner->input_names().empty());
    TfLiteTensor* input_tensor = runner->input_tensor(runner->input_names()[0]);

    if (input_tensor && input_tensor->dims && input_tensor->dims->size == 2) {
      // SAFETY: The `if` condition explicitly checks that
      // `input_tensor->dims->size` is exactly 2. Therefore, accessing indices 0
      // and 1 of the `data` array is guaranteed to be within bounds.
      // TfLiteIntArray is a third-party C struct, so we cannot use a safe C++
      // container here.
      size_t batch_size = base::checked_cast<size_t>(
          UNSAFE_BUFFERS(input_tensor->dims->data[0]));
      size_t sequence_length = base::checked_cast<size_t>(
          UNSAFE_BUFFERS(input_tensor->dims->data[1]));
      if (batch_size == 1 && sequence_length >= kControlTokensCount) {
        available_signatures.emplace_back(sequence_length, *key);
      }
    }
  }

  CHECK(!available_signatures.empty());

  // Sort signatures by sequence length.
  std::sort(available_signatures.begin(), available_signatures.end(),
            [](const ModelSignature& a, const ModelSignature& b) {
              return a.sequence_length < b.sequence_length;
            });
  return available_signatures;
}

GemmaModelExecutor::GemmaModelExecutor(
    int bos_id,
    int eos_id,
    int pad_id,
    std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine)
    : tflite_engine_(std::move(tflite_engine)),
      bos_id_(bos_id),
      eos_id_(eos_id),
      pad_id_(pad_id) {
  CHECK(tflite_engine_);
  CHECK(tflite_engine_->interpreter());
  available_signatures_ = ExtractSignatures(tflite_engine_.get());
}
GemmaModelExecutor::~GemmaModelExecutor() = default;

std::optional<EmbedderExecutionResult> GemmaModelExecutor::Execute(
    const std::vector<int>& raw_tokens) {
  const ModelSignature& signature = GetSignature(
      raw_tokens.size() + kControlTokensCount, available_signatures_);
  std::vector<int> formatted = FormatInput(
      raw_tokens, signature.sequence_length, bos_id_, eos_id_, pad_id_);

  tflite::Interpreter* interpreter = tflite_engine_->interpreter();
  tflite::SignatureRunner* runner =
      interpreter->GetSignatureRunner(signature.name.c_str());

  if (last_signature_name_ != signature.name) {
    CHECK_EQ(runner->AllocateTensors(), kTfLiteOk);
    last_signature_name_ = signature.name;
  }

  CHECK(!runner->input_names().empty());
  TfLiteTensor* input_tensor = runner->input_tensor(runner->input_names()[0]);
  CHECK(tflite::task::core::PopulateTensor<int>(formatted, input_tensor).ok());

  CHECK_EQ(runner->Invoke(), kTfLiteOk);

  CHECK(!runner->output_names().empty());
  const TfLiteTensor* output_tensor =
      runner->output_tensor(runner->output_names()[0]);
  std::vector<float> output;
  CHECK(tflite::task::core::PopulateVector<float>(output_tensor, &output).ok());

  return EmbedderExecutionResult{
      std::move(output), static_cast<uint32_t>(signature.sequence_length)};
}

// static
std::vector<int> GemmaModelExecutor::FormatInput(
    const std::vector<int>& raw_tokens,
    size_t signature_length,
    int bos_id,
    int eos_id,
    int pad_id) {
  std::vector<int> tokens;
  tokens.reserve(signature_length);

  // 1. Add Start-of-Sequence
  tokens.push_back(bos_id);

  // 2. Add tokens (with truncation if they don't fit in the signature)
  CHECK_GE(signature_length, kControlTokensCount);
  const size_t max_content = signature_length - kControlTokensCount;
  const size_t content_to_copy = std::min(raw_tokens.size(), max_content);
  tokens.insert(tokens.end(), raw_tokens.begin(),
                raw_tokens.begin() + content_to_copy);

  // 3. Add End-of-Sequence
  tokens.push_back(eos_id);

  // 4. Pad with pad_id to reach the exact signature length
  tokens.resize(signature_length, pad_id);

  return tokens;
}

// static
const GemmaModelExecutor::ModelSignature& GemmaModelExecutor::GetSignature(
    size_t sequence_length,
    const std::vector<GemmaModelExecutor::ModelSignature>&
        available_signatures) {
  for (const ModelSignature& sig : available_signatures) {
    if (sequence_length <= sig.sequence_length) {
      return sig;
    }
  }
  return available_signatures.back();
}

}  // namespace passage_embeddings
