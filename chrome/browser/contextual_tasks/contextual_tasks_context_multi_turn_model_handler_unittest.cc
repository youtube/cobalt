// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_multi_turn_model_handler.h"

#include "base/files/file_util.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/tab_relevance_model_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

namespace {

class ContextualTasksMultiTurnModelProvider
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  ContextualTasksMultiTurnModelProvider() {
    base::FilePath test_data_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
    model_file_path_ = test_data_dir.AppendASCII("components")
                           .AppendASCII("test")
                           .AppendASCII("data")
                           .AppendASCII("contextual_tasks")
                           .AppendASCII("multi_turn_tab_relevance.tflite");
  }

  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& any,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_MULTI_TURN_TAB_RELEVANCE) {
      auto model_metadata = optimization_guide::TestModelInfoBuilder()
                                .SetModelFilePath(model_file_path_)
                                .SetModelMetadata(model_metadata_)
                                .Build();
      observer->OnModelUpdated(optimization_target, *model_metadata);
      model_observers_.AddObserver(observer);
    }
  }

  const base::FilePath& model_file_path() const { return model_file_path_; }

  void SetModelMetadata(const optimization_guide::proto::Any& model_metadata) {
    model_metadata_ = model_metadata;
    auto model_info = optimization_guide::TestModelInfoBuilder()
                          .SetModelFilePath(model_file_path_)
                          .SetModelMetadata(model_metadata_)
                          .Build();
    model_observers_.Notify(
        &optimization_guide::OptimizationTargetModelObserver::OnModelUpdated,
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_MULTI_TURN_TAB_RELEVANCE,
        *model_info);
  }

 private:
  base::ObserverList<optimization_guide::OptimizationTargetModelObserver>
      model_observers_;
  base::FilePath model_file_path_;
  optimization_guide::proto::Any model_metadata_;
};

}  // namespace

class ContextualTasksContextMultiTurnModelHandlerTest : public testing::Test {
 public:
  ContextualTasksContextMultiTurnModelHandlerTest() = default;
  ~ContextualTasksContextMultiTurnModelHandlerTest() override = default;

  void SetUp() override {
    model_provider_ = std::make_unique<ContextualTasksMultiTurnModelProvider>();
    model_handler_ =
        std::make_unique<ContextualTasksContextMultiTurnModelHandler>(
            model_provider_.get(), task_environment_.GetMainThreadTaskRunner());
    ASSERT_TRUE(base::PathExists(model_provider_->model_file_path()));
  }

  void SetModelMetadata(
      const optimization_guide::proto::TabRelevanceModelMetadata& metadata) {
    optimization_guide::proto::Any any;
    any.set_type_url("type.googleapis.com/TabRelevanceModelMetadata");
    metadata.SerializeToString(any.mutable_value());
    model_provider_->SetModelMetadata(any);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return model_handler_->GetModelInfo().has_value(); }));
  }

  void TearDown() override {
    model_handler_.reset();
    model_provider_.reset();

    // Enqueue a QuitClosure immediately after the DeleteSoon cleanup tasks
    // to ensure the executor is fully destroyed before test exit.
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  ContextualTasksContextMultiTurnModelHandler* model_handler() const {
    return model_handler_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ContextualTasksMultiTurnModelProvider> model_provider_;
  std::unique_ptr<ContextualTasksContextMultiTurnModelHandler> model_handler_;
};

TEST_F(ContextualTasksContextMultiTurnModelHandlerTest,
       ExtractMultiTurnModelFeatures) {
  optimization_guide::proto::TabRelevanceModelMetadata metadata;
  metadata.set_num_conversation_thread_turns(1);
  metadata.set_max_titles_per_thread(1);
  metadata.set_num_embedding_dimensions(4);
  metadata.set_num_passages_per_tab(1);

  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CONVERSATION_THREAD_QUERIES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_TITLE_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_TITLE_EMBEDDING);

  QueryStateSignals query_signals;
  query_signals.query_embedding = {0.1f, 0.2f,
                                   0.3f};  // Expected 4, 4th padded.
  query_signals.conversation_thread_queries_embeddings.push_back(
      {0.4f, 0.5f});  // Expected 4, 3rd/4th padded.
  query_signals.context_tab_title_embedding = {0.6f, 0.7f, 0.8f, 0.9f};

  TabSignals tab_signals;
  tab_signals.candidate_title_embedding = {1.0f, 1.1f, 1.2f, 1.3f};

  std::vector<float> features =
      ContextualTasksContextMultiTurnModelHandler::ExtractModelFeatures(
          metadata, query_signals, tab_signals);

  ASSERT_EQ(features.size(), 16u);
  EXPECT_EQ(features[0], 0.1f);   // Query embedding [0]
  EXPECT_EQ(features[1], 0.2f);   // Query embedding [1]
  EXPECT_EQ(features[2], 0.3f);   // Query embedding [2]
  EXPECT_EQ(features[3], 0.0f);   // Query embedding [3] (padded)
  EXPECT_EQ(features[4], 0.4f);   // Conversation query embedding [0]
  EXPECT_EQ(features[5], 0.5f);   // Conversation query embedding [1]
  EXPECT_EQ(features[6], 0.0f);   // Conversation query embedding [2] (padded)
  EXPECT_EQ(features[7], 0.0f);   // Conversation query embedding [3] (padded)
  EXPECT_EQ(features[8], 0.6f);   // Active title embedding [0]
  EXPECT_EQ(features[9], 0.7f);   // Active title embedding [1]
  EXPECT_EQ(features[10], 0.8f);  // Active title embedding [2]
  EXPECT_EQ(features[11], 0.9f);  // Active title embedding [3]
  EXPECT_EQ(features[12], 1.0f);  // Candidate title embedding [0]
  EXPECT_EQ(features[13], 1.1f);  // Candidate title embedding [1]
  EXPECT_EQ(features[14], 1.2f);  // Candidate title embedding [2]
  EXPECT_EQ(features[15], 1.3f);  // Candidate title embedding [3]
}

// TODO(524489645): failing on Linux UBSan Tests.
#if defined(UNDEFINED_SANITIZER)
#define MAYBE_ExecuteModelWithSignals DISABLED_ExecuteModelWithSignals
#else
#define MAYBE_ExecuteModelWithSignals ExecuteModelWithSignals
#endif  // defined(UNDEFINED_SANITIZER)
TEST_F(ContextualTasksContextMultiTurnModelHandlerTest,
       MAYBE_ExecuteModelWithSignals) {
  ContextualTasksContextMultiTurnModelHandler* handler = model_handler();

  optimization_guide::proto::TabRelevanceModelMetadata metadata;
  metadata.set_num_conversation_thread_turns(1);
  metadata.set_max_titles_per_thread(1);
  metadata.set_num_embedding_dimensions(4);
  metadata.set_num_passages_per_tab(1);

  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CONVERSATION_THREAD_QUERIES_EMBEDDINGS);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_TITLE_EMBEDDING);
  metadata.add_input_feature_sequence(
      optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_CANDIDATE_TAB_TITLE_EMBEDDING);
  SetModelMetadata(metadata);

  QueryStateSignals query_signals;
  query_signals.query_embedding = {0.1f, 0.2f, 0.3f, 0.4f};
  query_signals.conversation_thread_queries_embeddings.push_back(
      {0.4f, 0.5f, 0.6f, 0.7f});
  query_signals.context_tab_title_embedding = {0.6f, 0.7f, 0.8f, 0.9f};

  std::vector<TabSignals> batch_tab_signals(1);
  batch_tab_signals[0].candidate_title_embedding = {1.0f, 1.1f, 1.2f, 1.3f};

  base::test::TestFuture<const std::vector<std::optional<std::vector<float>>>&,
                         const std::vector<std::vector<float>>&>
      future;
  handler->BatchExecuteModelWithSignalsForConversationThread(
      query_signals, batch_tab_signals, future.GetCallback());

  const auto& results = future.Get<0>();
  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0].has_value());
  ASSERT_EQ(results[0]->size(), 5u);
  EXPECT_NEAR((*results[0])[0], 0.1f, 1e-5);
  EXPECT_NEAR((*results[0])[1], 0.2f, 1e-5);
  EXPECT_NEAR((*results[0])[2], 0.3f, 1e-5);
  EXPECT_NEAR((*results[0])[3], 0.4f, 1e-5);
  EXPECT_NEAR((*results[0])[4], 0.5f, 1e-5);

  const auto& features = future.Get<1>();
  ASSERT_EQ(features.size(), 1u);
  EXPECT_EQ(features[0].size(), 16u);
}

}  // namespace contextual_tasks
