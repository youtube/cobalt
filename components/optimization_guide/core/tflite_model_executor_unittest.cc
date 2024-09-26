// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/core/test_tflite_model_executor.h"
#include "components/optimization_guide/core/test_tflite_model_handler.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace optimization_guide {
namespace {

class NoUnloadingTestTFLiteModelHandler : public TestTFLiteModelHandler {
 public:
  NoUnloadingTestTFLiteModelHandler(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : TestTFLiteModelHandler(model_provider,
                               background_task_runner,
                               std::make_unique<TestTFLiteModelExecutor>()) {
    SetShouldUnloadModelOnComplete(false);
  }
};

class EnsureCancelledTestTFLiteModelExecutor : public TestTFLiteModelExecutor {
 protected:
  absl::optional<std::vector<float>> Execute(
      ModelExecutionTask* execution_task,
      ExecutionStatus* out_status,
      const std::vector<float>& input) override {
    while (true) {
      // Timing is tricky, so give a few invocations for the cancel flag in
      // TFLite Support to be noticed.
      absl::optional<std::vector<float>> out =
          TestTFLiteModelExecutor::Execute(execution_task, out_status, input);
      if (!out) {
        return out;
      }
    }
  }
};

class EnsureCancelledTestTFLiteModelHandler : public TestTFLiteModelHandler {
 public:
  EnsureCancelledTestTFLiteModelHandler(
      OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : TestTFLiteModelHandler(
            model_provider,
            background_task_runner,
            std::make_unique<EnsureCancelledTestTFLiteModelExecutor>()) {}
};

class TFLiteModelExecutorTest : public testing::Test {
 public:
  TFLiteModelExecutorTest() = default;
  ~TFLiteModelExecutorTest() override = default;

  void SetUp() override {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    model_file_path_ = source_root_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("optimization_guide")
                           .AppendASCII("simple_test.tflite");

    test_model_provider_ =
        std::make_unique<TestOptimizationGuideModelProvider>();

    execution_sequence_ =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  }

  void TearDown() override { ResetModelHandler(); }

  virtual void CreateModelHandler() {
    if (model_handler_)
      model_handler_.reset();

    model_handler_ = std::make_unique<TestTFLiteModelHandler>(
        test_model_provider(), execution_sequence_);
  }

  void ResetModelHandler(
      std::unique_ptr<TestTFLiteModelHandler> handle = nullptr) {
    model_handler_ = std::move(handle);
    // Allow for the background class to be destroyed.
    RunUntilIdle();
  }

  void PushModelFileToModelExecutor(
      proto::OptimizationTarget optimization_target,
      const absl::optional<proto::Any>& model_metadata) {
    DCHECK(model_handler());
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetModelFilePath(model_file_path_)
            .SetModelMetadata(model_metadata)
            .Build();
    model_handler()->OnModelUpdated(optimization_target, *model_info);
    RunUntilIdle();
  }

  TestTFLiteModelHandler* model_handler() { return model_handler_.get(); }

  TestOptimizationGuideModelProvider* test_model_provider() {
    return test_model_provider_.get();
  }

  base::SequencedTaskRunner* execution_sequence() {
    return execution_sequence_.get();
  }
  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  std::unique_ptr<TestTFLiteModelHandler> model_handler_;

 private:
  scoped_refptr<base::SequencedTaskRunner> execution_sequence_;
  base::test::TaskEnvironment task_environment_;
  base::FilePath model_file_path_;
  std::unique_ptr<TestOptimizationGuideModelProvider> test_model_provider_;
};

TEST_F(TFLiteModelExecutorTest, ExecuteReturnsImmediatelyIfNoModelLoaded) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      std::vector<float>{1, 1, 1});
  run_loop->Run();

  // Ensures pending tasks are processed. They are generating UMA metrics.
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      ExecutionStatus::kErrorModelFileNotAvailable, 1);
}

TEST_F(TFLiteModelExecutorTest, ExecuteWithLoadedModel) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt);
  EXPECT_TRUE(model_handler()->ModelAvailable());

  std::vector<float> input;
  int expected_dims = 1 * 32 * 32 * 3;
  input.reserve(expected_dims);
  for (int i = 0; i < expected_dims; i++)
    input.emplace_back(1);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());

            std::vector<float> expected_output = {
                -0.4936581, -0.32497078, -0.1705023, -0.38193324, 0.36136785,
                0.2177353,  0.32200375,  0.28686714, -0.21846706, -0.4200018};
            for (size_t i = 0; i < expected_output.size(); i++)
              EXPECT_NEAR(expected_output[i], output.value()[i], 1e-5);

            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  // Ensures pending tasks are processed. They are generating UMA metrics.
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      ExecutionStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadedSuccessfully." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);
}

TEST_F(TFLiteModelExecutorTest, ExecuteTwiceWithLoadedModel) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt);
  EXPECT_TRUE(model_handler()->ModelAvailable());

  std::vector<float> input;
  int expected_dims = 1 * 32 * 32 * 3;
  input.reserve(expected_dims);
  for (int i = 0; i < expected_dims; i++)
    input.emplace_back(1);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  // First run.
  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  // Ensures pending tasks are processed. They are generating UMA metrics.
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TimeSincePreviousRun." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      ExecutionStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadedSuccessfully." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);

  // Second run.
  run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  // Ensures pending tasks are processed. They are generating UMA metrics.
  RunUntilIdle();

  // The model should have been loaded a second time.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      ExecutionStatus::kSuccess, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadedSuccessfully." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 2);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      2);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      2);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TimeSincePreviousRun." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
}

TEST_F(TFLiteModelExecutorTest, DoNotUnloadAfterExecution) {
  base::HistogramTester histogram_tester;
  ResetModelHandler(std::make_unique<NoUnloadingTestTFLiteModelHandler>(
      test_model_provider(), execution_sequence()));

  proto::Any any_metadata;
  any_metadata.set_type_url("type.googleapis.com/com.foo.Duration");
  proto::Duration model_metadata;
  model_metadata.set_seconds(123);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      any_metadata);

  EXPECT_TRUE(model_handler()->ModelAvailable());

  // While the model isn't actually loaded yet, the supported features are
  // already known and do not change when the model is loaded or unloaded.
  EXPECT_TRUE(model_handler()
                  ->ParsedSupportedFeaturesForLoadedModel<proto::Duration>());

  std::vector<float> input;
  size_t expected_dims = 1 * 32 * 32 * 3;
  input.reserve(expected_dims);
  for (size_t i = 0; i < expected_dims; i++) {
    input.emplace_back(1);
  }
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  // Ensures pending tasks are processed. They are generating UMA metrics.
  RunUntilIdle();

  EXPECT_TRUE(model_handler()->ModelAvailable());
  EXPECT_TRUE(model_handler()
                  ->ParsedSupportedFeaturesForLoadedModel<proto::Duration>());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      ExecutionStatus::kSuccess, 1);

  // Run again and do not expect a second model load histogram count.
  run_loop = std::make_unique<base::RunLoop>();
  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_TRUE(output.has_value());
            run_loop->Quit();
          },
          run_loop.get()),
      input);
  run_loop->Run();

  // Ensures pending tasks are processed. They are generating UMA metrics.
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.TaskSchedulingLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      2);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ExecutionLatency." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);
}

class CancelledTFLiteModelExecutorTest : public TFLiteModelExecutorTest {
 public:
  CancelledTFLiteModelExecutorTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPreventLongRunningPredictionModels,
        {{"model_execution_timeout_ms", "10"}});
  }
  ~CancelledTFLiteModelExecutorTest() override = default;

  void CreateModelHandler() override {
    model_handler_ = std::make_unique<EnsureCancelledTestTFLiteModelHandler>(
        test_model_provider(), execution_sequence());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CancelledTFLiteModelExecutorTest, RunsTooLong) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt);
  EXPECT_TRUE(model_handler()->ModelAvailable());

  std::vector<float> input;
  size_t expected_dims = 1 * 32 * 32 * 3;
  input.reserve(expected_dims);
  for (size_t i = 0; i < expected_dims; i++) {
    input.emplace_back(1);
  }

  base::RunLoop run_loop;
  model_handler()->ExecuteModelWithInput(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const absl::optional<std::vector<float>>& output) {
            EXPECT_FALSE(output.has_value());
            run_loop->Quit();
          },
          &run_loop),
      input);
  run_loop.Run();

  // Ensures pending tasks are processed. They are generating UMA metrics.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PainfulPageLoad",
      ExecutionStatus::kErrorCancelled, 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.DidTimeout.PainfulPageLoad", true, 1);
}

TEST_F(TFLiteModelExecutorTest, UpdateModelFileWithPreloading) {
  base::HistogramTester histogram_tester;
  CreateModelHandler();

  model_handler_->SetShouldUnloadModelOnComplete(false);
  // Invoke UpdateModelFile() to preload model.
  PushModelFileToModelExecutor(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      /*model_metadata=*/absl::nullopt);

  // Ensures pending tasks are processed. They are generating UMA metrics.
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelLoadedSuccessfully." +
          optimization_guide::GetStringNameForOptimizationTarget(
              proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD),
      true, 1);
}

}  // namespace
}  // namespace optimization_guide
