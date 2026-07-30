// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_EXECUTOR_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_EXECUTOR_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tflite::task::core {
class TfLiteEngine;
}  // namespace tflite::task::core

namespace passage_embeddings {

class PassageEmbedderExecutionTask;

struct EmbedderExecutionResult {
  std::vector<float> embeddings;
  uint32_t signature_length = 0;
};

class PassageEmbedderExecutor {
 public:
  PassageEmbedderExecutor() = default;
  PassageEmbedderExecutor(const PassageEmbedderExecutor&) = delete;
  PassageEmbedderExecutor& operator=(const PassageEmbedderExecutor&) = delete;
  virtual ~PassageEmbedderExecutor() = default;

  virtual std::optional<EmbedderExecutionResult> Execute(
      const std::vector<int>& raw_tokens) = 0;
};

class HistoryModelExecutor : public PassageEmbedderExecutor {
 public:
  HistoryModelExecutor(
      uint32_t input_window_size,
      int eos_id,
      std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine);
  HistoryModelExecutor(const HistoryModelExecutor&) = delete;
  HistoryModelExecutor& operator=(const HistoryModelExecutor&) = delete;
  ~HistoryModelExecutor() override;

  std::optional<EmbedderExecutionResult> Execute(
      const std::vector<int>& raw_tokens) override;

  // Public for testing.
  static std::vector<int> FormatInput(const std::vector<int>& input,
                                      uint32_t input_window_size,
                                      int eos_id);

 private:
  const std::unique_ptr<PassageEmbedderExecutionTask> task_;
  const uint32_t input_window_size_;
  const int eos_id_;
};

class GemmaModelExecutor : public PassageEmbedderExecutor {
 public:
  struct ModelSignature {
    size_t sequence_length;
    std::string name;
  };

  static constexpr size_t kControlTokensCount = 2;

  GemmaModelExecutor(
      int bos_id,
      int eos_id,
      int pad_id,
      std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine);
  GemmaModelExecutor(const GemmaModelExecutor&) = delete;
  GemmaModelExecutor& operator=(const GemmaModelExecutor&) = delete;
  ~GemmaModelExecutor() override;

  std::optional<EmbedderExecutionResult> Execute(
      const std::vector<int>& raw_tokens) override;

  // Public for testing.
  static std::vector<int> FormatInput(const std::vector<int>& raw_tokens,
                                      size_t signature_length,
                                      int bos_id,
                                      int eos_id,
                                      int pad_id);

  // Public for testing.
  static const ModelSignature& GetSignature(
      size_t sequence_length,
      const std::vector<ModelSignature>& available_signatures);

 private:
  static std::vector<ModelSignature> ExtractSignatures(
      tflite::task::core::TfLiteEngine* tflite_engine);

  const std::unique_ptr<tflite::task::core::TfLiteEngine> tflite_engine_;
  const int bos_id_;
  const int eos_id_;
  const int pad_id_;
  std::vector<ModelSignature> available_signatures_;
  std::string last_signature_name_ = "";
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDER_EXECUTOR_H_
