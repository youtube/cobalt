// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/passage_embeddings/passage_embedder_executor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "services/passage_embeddings/passage_embeddings_op_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/tflite_engine.h"

namespace passage_embeddings {

using testing::ElementsAre;

class PassageEmbedderExecutorTest : public testing::Test {
 public:
  PassageEmbedderExecutorTest() = default;
  ~PassageEmbedderExecutorTest() override = default;

  void SetUp() override {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("services")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("passage_embeddings");
    history_model_path_ =
        test_data_dir.AppendASCII("dummy_embeddings_model.tflite");
    // dummy_gemma_model.tflite is a mock TFLite model containing two
    // signatures:
    // - "sig_10": Input tensor `[1, 10]` of type int32.
    // - "sig_20": Input tensor `[1, 20]` of type int32.
    // It is used to test the GemmaModelExecutor's ability to dynamically
    // extract and route tokens to the correct signature based on sequence
    // length.
    gemma_model_path_ = test_data_dir.AppendASCII("dummy_gemma_model.tflite");
    base::FilePath opt_guide_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &opt_guide_dir);
    simple_model_path_ = opt_guide_dir.AppendASCII("components")
                             .AppendASCII("test")
                             .AppendASCII("data")
                             .AppendASCII("optimization_guide")
                             .AppendASCII("simple_test.tflite");
  }

  std::unique_ptr<tflite::task::core::TfLiteEngine> BuildEngine(
      const base::FilePath& path,
      std::unique_ptr<optimization_guide::TFLiteOpResolver> resolver) {
    auto engine =
        std::make_unique<tflite::task::core::TfLiteEngine>(std::move(resolver));
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid()) {
      return nullptr;
    }
#if BUILDFLAG(IS_WIN)
    auto status = engine->BuildModelFromFileHandle(file.GetPlatformFile());
#else
    auto status = engine->BuildModelFromFileDescriptor(file.GetPlatformFile());
#endif
    if (!status.ok()) {
      return nullptr;
    }
    status = engine->InitInterpreter();
    if (!status.ok()) {
      return nullptr;
    }
    return engine;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::FilePath history_model_path_;
  base::FilePath gemma_model_path_;
  base::FilePath simple_model_path_;
};

TEST_F(PassageEmbedderExecutorTest, HistoryModelFormatInput) {
  // Input window size = 5, EOS ID = 99
  // 1. Short sequence: Should append EOS and pad with 0s up to the window size.
  std::vector<int> short_seq = {10, 11, 12};
  EXPECT_THAT(HistoryModelExecutor::FormatInput(short_seq, 5, 99),
              ElementsAre(10, 11, 12, 99, 0));

  // 2. Exact-length boundary.
  // HISTORY EMBEDDINGS BEHAVIOR: If input size equals the window size, the EOS
  // token is skipped, and no padding is applied. We explicitly test for this
  // behavior as the History Embeddings model implicitly relies on this
  // truncation.
  std::vector<int> exact_seq = {10, 11, 12, 13, 14};
  EXPECT_THAT(HistoryModelExecutor::FormatInput(exact_seq, 5, 99),
              ElementsAre(10, 11, 12, 13, 14));

  // 3. Over-length truncation.
  // HISTORY EMBEDDINGS BEHAVIOR: If input size exceeds window size, elements
  // are truncated at the end, entirely omitting the EOS token.
  std::vector<int> long_seq = {10, 11, 12, 13, 14, 15, 16};
  EXPECT_THAT(HistoryModelExecutor::FormatInput(long_seq, 5, 99),
              ElementsAre(10, 11, 12, 13, 14));
}

TEST_F(PassageEmbedderExecutorTest, HistoryModelFormatInputEmpty) {
  EXPECT_THAT(HistoryModelExecutor::FormatInput({}, 5, 99),
              ElementsAre(99, 0, 0, 0, 0));
}

TEST_F(PassageEmbedderExecutorTest, GemmaModelGetSignature) {
  std::vector<GemmaModelExecutor::ModelSignature> signatures = {
      {10, "sig_10"}, {20, "sig_20"}, {30, "sig_30"}};

  // Fits well within the first signature (10).
  const GemmaModelExecutor::ModelSignature& sig1 =
      GemmaModelExecutor::GetSignature(8, signatures);
  EXPECT_EQ(sig1.sequence_length, 10u);
  EXPECT_EQ(sig1.name, "sig_10");

  // Exact boundary fit for the second signature (20).
  const GemmaModelExecutor::ModelSignature& sig2 =
      GemmaModelExecutor::GetSignature(20, signatures);
  EXPECT_EQ(sig2.sequence_length, 20u);
  EXPECT_EQ(sig2.name, "sig_20");

  // Exceeds the max signature length (30); falls back safely to the largest
  // available.
  const GemmaModelExecutor::ModelSignature& sig3 =
      GemmaModelExecutor::GetSignature(40, signatures);
  EXPECT_EQ(sig3.sequence_length, 30u);
  EXPECT_EQ(sig3.name, "sig_30");
}

TEST_F(PassageEmbedderExecutorTest, GemmaModelFormatInput) {
  // BOS ID = 1, EOS ID = 2, PAD ID = 0

  // 1. Short sequence requiring padding.
  // Input length 2 requires total length 4 (BOS + 2 + EOS). Fits inside
  // signature length 5. Expectation: BOS, 10, 11, EOS, PAD
  std::vector<int> formatted1 = GemmaModelExecutor::FormatInput(
      {10, 11}, 5, /*bos_id=*/1, /*eos_id=*/2, /*pad_id=*/0);
  EXPECT_THAT(formatted1, ElementsAre(1, 10, 11, 2, 0));

  // 2. Exact boundary fit.
  // Input length 3 requires total length 5. Fits exactly in sig_5.
  // Expectation: BOS, 10, 11, 12, EOS
  std::vector<int> formatted2 = GemmaModelExecutor::FormatInput(
      {10, 11, 12}, 5, /*bos_id=*/1, /*eos_id=*/2, /*pad_id=*/0);
  EXPECT_THAT(formatted2, ElementsAre(1, 10, 11, 12, 2));

  // 3. Over-length truncation.
  // Input length 10 requires total 12. Max sig is 10.
  // The system must intelligently truncate the raw tokens to leave room for BOS
  // + EOS. Max raw tokens we can fit: 10 - 2 = 8. Expectation: BOS, 8
  // tokens..., EOS
  std::vector<int> formatted3 = GemmaModelExecutor::FormatInput(
      {10, 11, 12, 13, 14, 15, 16, 17, 18, 19}, 10, /*bos_id=*/1, /*eos_id=*/2,
      /*pad_id=*/0);
  EXPECT_THAT(formatted3, ElementsAre(1, 10, 11, 12, 13, 14, 15, 16, 17, 2));
}

TEST_F(PassageEmbedderExecutorTest, GemmaModelFormatInputEmpty) {
  std::vector<int> formatted = GemmaModelExecutor::FormatInput(
      {}, 10, /*bos_id=*/1, /*eos_id=*/2, /*pad_id=*/0);
  EXPECT_THAT(formatted, ElementsAre(1, 2, 0, 0, 0, 0, 0, 0, 0, 0));
}

TEST_F(PassageEmbedderExecutorTest, HistoryModelExecutorSucceeds) {
  std::unique_ptr<tflite::task::core::TfLiteEngine> engine = BuildEngine(
      history_model_path_,
      std::make_unique<HistoryOpResolver>(/*allow_gpu_execution=*/false));
  ASSERT_TRUE(engine);
  HistoryModelExecutor executor(256, 99, std::move(engine));
  auto result = executor.Execute({1, 2, 3});
  EXPECT_TRUE(result.has_value());
}

TEST_F(PassageEmbedderExecutorTest, GemmaModelExecutorSucceeds) {
  std::unique_ptr<tflite::task::core::TfLiteEngine> engine =
      BuildEngine(gemma_model_path_, std::make_unique<GemmaOpResolver>());
  ASSERT_TRUE(engine);
  GemmaModelExecutor executor(1, 2, 0, std::move(engine));
  auto result = executor.Execute({10, 11, 12});
  EXPECT_TRUE(result.has_value());
}

}  // namespace passage_embeddings
