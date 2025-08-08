// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/ml_answerer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

namespace {

using ::base::test::TestFuture;
using ::optimization_guide::AnyWrapProto;
using ::optimization_guide::MockSession;
using ::optimization_guide::OptimizationGuideModelExecutionError;
using ::optimization_guide::OptimizationGuideModelStreamingExecutionResult;
using ::optimization_guide::proto::HistoryAnswerResponse;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

}  // namespace

class MockModelExecutor
    : public optimization_guide::MockOptimizationGuideModelExecutor {
 public:
  size_t GetCounter() { return counter_; }

  void IncrementCounter() { counter_++; }

  void Reset() { counter_ = 0; }

 private:
  size_t counter_ = 0;
};

class HistoryEmbeddingsMlAnswererTest : public testing::Test {
 public:
  void SetUp() override {
    ml_answerer_ = std::make_unique<MlAnswerer>(&model_executor_);
    token_limits_ = {
        .min_context_tokens = 1024,
    };


    ON_CALL(session_1_, GetTokenLimits())
        .WillByDefault([&]() -> optimization_guide::TokenLimits& {
          return GetTokenLimits();
        });
    ON_CALL(session_2_, GetTokenLimits())
        .WillByDefault([&]() -> optimization_guide::TokenLimits& {
          return GetTokenLimits();
        });
  }

  optimization_guide::TokenLimits& GetTokenLimits() { return token_limits_; }

 protected:
  optimization_guide::StreamingResponse MakeResponse(
      const std::string& answer_text,
      bool is_complete = true) {
    HistoryAnswerResponse answer_response;
    answer_response.mutable_answer()->set_text(answer_text);
    return optimization_guide::StreamingResponse{
        .response = AnyWrapProto(answer_response),
        .is_complete = is_complete,
    };
  }

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<MockModelExecutor> model_executor_;
  std::unique_ptr<MlAnswerer> ml_answerer_;
  testing::NiceMock<optimization_guide::MockSession> session_1_, session_2_;
  optimization_guide::TokenLimits token_limits_;
};

TEST_F(HistoryEmbeddingsMlAnswererTest, ComputeAnswerNoSession) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    return nullptr;
  });

  ON_CALL(session_1_, GetSizeInTokens(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(100); })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});
  base::test::TestFuture<AnswererResult> result_future;
  ml_answerer_->ComputeAnswer("query", context, result_future.GetCallback());

  AnswererResult result = result_future.Take();
  EXPECT_EQ(ComputeAnswerStatus::kModelUnavailable, result.status);
}

#if !BUILDFLAG(IS_FUCHSIA)
TEST_F(HistoryEmbeddingsMlAnswererTest, ComputeAnswerExecutionFailure) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    return std::make_unique<NiceMock<MockSession>>(&session_1_);
  });

  ON_CALL(session_1_, GetSizeInTokens(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(100); })));

  ON_CALL(session_1_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.6); })));

  ON_CALL(session_1_, ExecuteModel(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    std::move(callback),
                    OptimizationGuideModelStreamingExecutionResult(
                        base::unexpected(
                            OptimizationGuideModelExecutionError::
                                FromModelExecutionError(
                                    OptimizationGuideModelExecutionError::
                                        ModelExecutionError::kGenericFailure)),
                        /*provided_by_on_device=*/true, nullptr)));
          })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});
  base::test::TestFuture<AnswererResult> result_future;
  ml_answerer_->ComputeAnswer("query", context, result_future.GetCallback());

  AnswererResult result = result_future.Take();
  EXPECT_EQ(ComputeAnswerStatus::kExecutionFailure, result.status);
}
#endif

TEST_F(HistoryEmbeddingsMlAnswererTest, ComputeAnswerSingleUrl) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    return std::make_unique<NiceMock<MockSession>>(&session_1_);
  });

  ON_CALL(session_1_, GetSizeInTokens(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(100); })));

  ON_CALL(session_1_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.6); })));

  ON_CALL(session_1_, ExecuteModel(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               OptimizationGuideModelStreamingExecutionResult(
                                   base::ok(MakeResponse("Answer_1")),
                                   /*provided_by_on_device=*/true, nullptr)));
          })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});

  base::test::TestFuture<AnswererResult> result_future;
  ml_answerer_->ComputeAnswer("query", context, result_future.GetCallback());

  AnswererResult answer_result = result_future.Take();
  EXPECT_EQ("Answer_1", answer_result.answer.text());
  EXPECT_EQ("url_1", answer_result.url);
}

TEST_F(HistoryEmbeddingsMlAnswererTest, ComputeAnswerMultipleUrls) {
  ON_CALL(model_executor_, StartSession(_, _))
      .WillByDefault([&]() -> std::unique_ptr<MockSession> {
        if (model_executor_.GetCounter() == 0) {
          model_executor_.IncrementCounter();
          return std::make_unique<NiceMock<MockSession>>(&session_1_);
        } else if (model_executor_.GetCounter() == 1) {
          model_executor_.IncrementCounter();
          return std::make_unique<NiceMock<MockSession>>(&session_2_);
        }
        return std::unique_ptr<StrictMock<MockSession>>();
      });

  ON_CALL(session_1_, GetSizeInTokens(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(100); })));

  ON_CALL(session_2_, GetSizeInTokens(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(100); })));

  ON_CALL(session_1_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.6); })));

  ON_CALL(session_2_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) {
            std::move(callback).Run(0.9);
          })));  // Speculative decoding should continue with this session.

  ON_CALL(session_2_, ExecuteModel(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::
                  OptimizationGuideModelExecutionResultStreamingCallback
                      callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               OptimizationGuideModelStreamingExecutionResult(
                                   base::ok(MakeResponse("Answer_2")),
                                   /*provided_by_on_device=*/true, nullptr)));
          })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});
  context.url_passages_map.insert({"url_2", {"passage_21", "passage_22"}});

  base::test::TestFuture<AnswererResult> result_future;
  ml_answerer_->ComputeAnswer("query", context, result_future.GetCallback());

  AnswererResult answer_result = result_future.Take();
  EXPECT_EQ("Answer_2", answer_result.answer.text());
  EXPECT_EQ("url_2", answer_result.url);
}

TEST_F(HistoryEmbeddingsMlAnswererTest, ComputeAnswerUnanswerable) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    return std::make_unique<NiceMock<MockSession>>(&session_1_);
  });

  ON_CALL(session_1_, GetSizeInTokens(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(100); })));

  // Below the default 0.5 threshold.
  ON_CALL(session_1_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(0.3); })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});

  TestFuture<AnswererResult> future;
  ml_answerer_->ComputeAnswer("query", context, future.GetCallback());
  const auto answer_result = future.Take();
  EXPECT_EQ(ComputeAnswerStatus::kUnanswerable, answer_result.status);
}

TEST_F(HistoryEmbeddingsMlAnswererTest, ComputeAnswerNullScores) {
  ON_CALL(model_executor_, StartSession(_, _)).WillByDefault([&] {
    return std::make_unique<NiceMock<MockSession>>(&session_1_);
  });

  ON_CALL(session_1_, GetSizeInTokens(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) { std::move(callback).Run(100); })));

  // Null score
  ON_CALL(session_1_, Score(_, _))
      .WillByDefault(testing::WithArg<1>(testing::Invoke(
          [&](optimization_guide::OptimizationGuideModelScoreCallback
                  callback) { std::move(callback).Run(std::nullopt); })));

  Answerer::Context context("1");
  context.url_passages_map.insert({"url_1", {"passage_11", "passage_12"}});

  TestFuture<AnswererResult> future;
  ml_answerer_->ComputeAnswer("query", context, future.GetCallback());
  const auto answer_result = future.Take();
  EXPECT_EQ(ComputeAnswerStatus::kExecutionFailure, answer_result.status);
}

}  // namespace history_embeddings
