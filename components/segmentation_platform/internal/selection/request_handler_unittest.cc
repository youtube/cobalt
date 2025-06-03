// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/selection/request_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/internal/post_processor/post_processing_test_utils.h"
#include "components/segmentation_platform/internal/selection/segment_result_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/proto/prediction_result.pb.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/trigger.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::FloatNear;
using testing::Invoke;
using testing::Return;

namespace segmentation_platform {
namespace {

// Test Ids.
const proto::SegmentId kSegmentId =
    proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;

class MockResultProvider : public SegmentResultProvider {
 public:
  MOCK_METHOD1(GetSegmentResult,
               void(std::unique_ptr<GetResultOptions> options));
};

class MockTrainingDataCollector : public TrainingDataCollector {
 public:
  MOCK_METHOD0(OnModelMetadataUpdated, void());
  MOCK_METHOD0(OnServiceInitialized, void());
  MOCK_METHOD0(ReportCollectedContinuousTrainingData, void());
  MOCK_METHOD4(
      OnDecisionTime,
      TrainingRequestId(proto::SegmentId id,
                        scoped_refptr<InputContext> input_context,
                        DecisionType type,
                        absl::optional<ModelProvider::Request> inputs));
  MOCK_METHOD4(CollectTrainingData,
               void(SegmentId segment_id,
                    TrainingRequestId request_id,
                    const TrainingLabels& param,
                    SuccessCallback callback));
};

proto::PredictionResult CreatePredictionResultWithBinaryClassifier() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForBinaryClassifier(0.5f, "positive_label",
                                            "negative_label");

  proto::PredictionResult result;
  result.add_result(0.8f);
  result.mutable_output_config()->Swap(model_metadata.mutable_output_config());
  return result;
}

proto::PredictionResult CreatePredictionResultWithGenericPredictor() {
  proto::SegmentationModelMetadata model_metadata;
  MetadataWriter writer(&model_metadata);
  writer.AddOutputConfigForGenericPredictor({"output1", "output2"});

  proto::PredictionResult prediction_result;
  prediction_result.add_result(0.8f);
  prediction_result.add_result(0.2f);
  prediction_result.mutable_output_config()->Swap(
      model_metadata.mutable_output_config());
  return prediction_result;
}

class RequestHandlerTest : public testing::Test {
 public:
  RequestHandlerTest() = default;
  ~RequestHandlerTest() override = default;

  void SetUp() override {
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
    auto training_data_collector =
        std::make_unique<MockTrainingDataCollector>();
    training_data_collector_ = training_data_collector.get();
    execution_service_.set_training_data_collector_for_testing(
        std::move(training_data_collector));
    config_ = test_utils::CreateTestConfig("test_client", kSegmentId);
    auto provider = std::make_unique<MockResultProvider>();
    result_provider_ = provider.get();
    request_handler_ = RequestHandler::Create(*config_, std::move(provider),
                                              &execution_service_);
  }

  void OnGetPredictionResult(base::RepeatingClosure closure,
                             const RawResult& result) {
    EXPECT_EQ(result.status, PredictionStatus::kSucceeded);
    EXPECT_NEAR(0.8, result.result.result(0), 0.001);
    EXPECT_EQ(result.request_id, TrainingRequestId::FromUnsafeValue(15));
    std::move(closure).Run();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<Config> config_;
  ExecutionService execution_service_;
  raw_ptr<MockTrainingDataCollector> training_data_collector_;
  std::unique_ptr<RequestHandler> request_handler_;
  raw_ptr<MockResultProvider> result_provider_ = nullptr;
};

TEST_F(RequestHandlerTest, GetPredictionResult) {
  PredictionOptions options;
  options.on_demand_execution = true;

  EXPECT_CALL(
      *training_data_collector_,
      OnDecisionTime(kSegmentId, _,
                     proto::TrainingOutputs::TriggerConfig::ONDEMAND,
                     absl::make_optional(ModelProvider::Request{1, 2, 3})))
      .WillOnce(Return(TrainingRequestId::FromUnsafeValue(15)));
  EXPECT_CALL(*result_provider_, GetSegmentResult(_))
      .WillOnce(Invoke(
          [](std::unique_ptr<SegmentResultProvider::GetResultOptions> options) {
            EXPECT_TRUE(options->ignore_db_scores);
            EXPECT_EQ(options->segment_id, kSegmentId);
            auto result =
                std::make_unique<SegmentResultProvider::SegmentResult>(
                    SegmentResultProvider::ResultState::
                        kServerModelExecutionScoreUsed,
                    CreatePredictionResultWithBinaryClassifier(),
                    /*rank=*/2);
            result->model_inputs = {1, 2, 3};
            std::move(options->callback).Run(std::move(result));
          }));

  base::RunLoop loop;
  RawResult result(PredictionStatus::kFailed);
  request_handler_->GetPredictionResult(
      options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestHandlerTest::OnGetPredictionResult,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

TEST_F(RequestHandlerTest, GetGenericPredictionResult) {
  PredictionOptions options;
  options.on_demand_execution = true;

  EXPECT_CALL(*training_data_collector_,
              OnDecisionTime(kSegmentId, _,
                             proto::TrainingOutputs::TriggerConfig::ONDEMAND,
                             absl::make_optional(ModelProvider::Request{1})))
      .WillOnce(Return(TrainingRequestId::FromUnsafeValue(15)));
  EXPECT_CALL(*result_provider_, GetSegmentResult(_))
      .WillOnce(Invoke(
          [](std::unique_ptr<SegmentResultProvider::GetResultOptions> options) {
            EXPECT_TRUE(options->ignore_db_scores);
            EXPECT_EQ(options->segment_id, kSegmentId);
            auto result =
                std::make_unique<SegmentResultProvider::SegmentResult>(
                    SegmentResultProvider::ResultState::
                        kServerModelExecutionScoreUsed,
                    CreatePredictionResultWithGenericPredictor(),
                    /*rank=*/2);
            result->model_inputs = {1};
            std::move(options->callback).Run(std::move(result));
          }));

  base::RunLoop loop;
  RawResult result(PredictionStatus::kFailed);
  request_handler_->GetPredictionResult(
      options, scoped_refptr<InputContext>(),
      base::BindOnce(&RequestHandlerTest::OnGetPredictionResult,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
}

}  // namespace
}  // namespace segmentation_platform
