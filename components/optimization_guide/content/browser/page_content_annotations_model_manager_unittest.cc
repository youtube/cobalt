// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_model_manager.h"

#include "base/containers/flat_map.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/page_entities_model_handler.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using ::testing::FloatEq;
using ::testing::UnorderedElementsAre;

class ModelObserverTracker : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      const absl::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    registered_model_metadata_.insert_or_assign(target, model_metadata);
  }

  bool DidRegisterForTarget(
      proto::OptimizationTarget target,
      absl::optional<proto::Any>* out_model_metadata) const {
    auto it = registered_model_metadata_.find(target);
    if (it == registered_model_metadata_.end())
      return false;
    if (out_model_metadata)
      *out_model_metadata = registered_model_metadata_.at(target);
    return true;
  }

 private:
  base::flat_map<proto::OptimizationTarget, absl::optional<proto::Any>>
      registered_model_metadata_;
};

class FakePageEntitiesModelHandler : public PageEntitiesModelHandler {
 public:
  explicit FakePageEntitiesModelHandler(
      const base::flat_map<std::string, std::vector<ScoredEntityMetadata>>&
          entries,
      const base::flat_map<std::string, EntityMetadata>& entity_metadata)
      : entries_(entries), entity_metadata_(entity_metadata) {}
  ~FakePageEntitiesModelHandler() override = default;

  void ExecuteModelWithInput(
      const std::string& text,
      PageEntitiesMetadataModelExecutedCallback callback) override {
    auto it = entries_.find(text);
    std::move(callback).Run(
        it != entries_.end() ? absl::make_optional(it->second) : absl::nullopt);
  }

  void GetMetadataForEntityId(
      const std::string& entity_id,
      PageEntitiesModelEntityMetadataRetrievedCallback callback) override {
    auto it = entity_metadata_.find(entity_id);
    std::move(callback).Run(it != entity_metadata_.end()
                                ? absl::make_optional(it->second)
                                : absl::nullopt);
  }

  void GetMetadataForEntityIds(
      const base::flat_set<std::string>& entity_ids,
      PageEntitiesModelBatchEntityMetadataRetrievedCallback callback) override {
    base::flat_map<std::string, EntityMetadata> entity_metadata_map;
    for (const auto& entity_id : entity_ids) {
      auto it = entity_metadata_.find(entity_id);
      if (it != entity_metadata_.end()) {
        entity_metadata_map[entity_id] = it->second;
      }
    }
    std::move(callback).Run(entity_metadata_map);
  }

  void AddOnModelUpdatedCallback(base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  absl::optional<ModelInfo> GetModelInfo() const override {
    return absl::nullopt;
  }

 private:
  base::flat_map<std::string, std::vector<ScoredEntityMetadata>> entries_;
  base::flat_map<std::string, EntityMetadata> entity_metadata_;
};

class PageContentAnnotationsModelManagerTest : public testing::Test {
 public:
  PageContentAnnotationsModelManagerTest() {
    // Enable Visibility but disable Entities.
    scoped_feature_list_.InitWithFeatures(
        {features::kPageVisibilityPageContentAnnotations},
        {features::kPageEntitiesPageContentAnnotations,
         features::kPreventLongRunningPredictionModels});
  }
  ~PageContentAnnotationsModelManagerTest() override = default;

  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
    model_manager_ = std::make_unique<PageContentAnnotationsModelManager>(
        model_observer_tracker_.get());
  }

  void TearDown() override {
    model_manager_.reset();
    model_observer_tracker_.reset();
  }

  proto::PageTopicsModelMetadata MakeValidPageTopicsModelMetadata() const {
    proto::PageTopicsModelMetadata page_topics_model_metadata;
    page_topics_model_metadata.set_version(123);
    page_topics_model_metadata.add_supported_output(
        proto::PageTopicsSupportedOutput::
            PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
    page_topics_model_metadata.mutable_output_postprocessing_params()
        ->mutable_category_params()
        ->set_max_categories(10);
    page_topics_model_metadata.mutable_output_postprocessing_params()
        ->mutable_category_params()
        ->set_min_category_weight(0);
    return page_topics_model_metadata;
  }

  void SetupPageTopicsV2ModelExecutor() {
    model_manager()->RequestAndNotifyWhenModelAvailable(
        AnnotationType::kPageTopics, base::DoNothing());
    // If the feature flag is disabled, the executor won't have been created so
    // skip everything else.
    if (!model_manager()->page_topics_model_handler_)
      return;

    proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.PageTopicsModelMetadata");
    MakeValidPageTopicsModelMetadata().SerializeToString(
        any_metadata.mutable_value());

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    // We know that the model executor itself works fine (that's tested
    // elsewhere), so just make sure that all the plumbing for the model
    // execution: job, queue, background sequences, etc, are working correctly.
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("non_existent_model.tflite");
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetModelFilePath(model_file_path)
            .SetModelMetadata(any_metadata)
            .Build();
    model_manager()->page_topics_model_handler_->OnModelUpdated(
        proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2, *model_info);
    RunUntilIdle();
  }

  void SendPageVisibilityModelToExecutor(
      const absl::optional<proto::Any>& model_metadata) {
    model_manager()->RequestAndNotifyWhenModelAvailable(
        AnnotationType::kContentVisibility, base::DoNothing());
    // If the feature flag is disabled, the executor won't have been created so
    // skip everything else.
    if (!model_manager()->page_visibility_model_handler_)
      return;

    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    // We know that the model executor itself works fine (that's tested
    // elsewhere), so just make sure that all the plumbing for the model
    // execution: job, queue, background sequences, etc, are working correctly.
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("non_existent_model.tflite");
    std::unique_ptr<ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetModelFilePath(model_file_path)
            .SetModelMetadata(model_metadata)
            .Build();
    model_manager()->page_visibility_model_handler_->OnModelUpdated(
        proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY, *model_info);
    RunUntilIdle();
  }

  void SetPageEntitiesModelHandler(
      const base::flat_map<std::string, std::vector<ScoredEntityMetadata>>&
          entries,
      const base::flat_map<std::string, EntityMetadata>& entity_metadata) {
    model_manager()->OverridePageEntitiesModelHandlerForTesting(
        std::make_unique<FakePageEntitiesModelHandler>(entries,
                                                       entity_metadata));
  }

  absl::optional<EntityMetadata> GetMetadataForEntityId(
      const std::string& entity_id) {
    absl::optional<EntityMetadata> entities_metadata;
    base::RunLoop run_loop;
    model_manager()->GetMetadataForEntityId(
        entity_id,
        base::BindOnce(
            [](base::RunLoop* run_loop,
               absl::optional<EntityMetadata>* out_entities_metadata,
               const absl::optional<EntityMetadata>& entities_metadata) {
              *out_entities_metadata = entities_metadata;
              run_loop->Quit();
            },
            &run_loop, &entities_metadata));
    run_loop.Run();

    return entities_metadata;
  }

  ModelObserverTracker* model_observer_tracker() const {
    return model_observer_tracker_.get();
  }

  PageContentAnnotationsModelManager* model_manager() const {
    return model_manager_.get();
  }

  base::HistogramTester* histogram_tester() const {
    return histogram_tester_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<PageContentAnnotationsModelManager> model_manager_;
};

TEST_F(PageContentAnnotationsModelManagerTest,
       GetMetadataForEntityIdEntitiesAnnotatorNotInitialized) {
  EXPECT_FALSE(GetMetadataForEntityId("someid").has_value());
}

TEST_F(PageContentAnnotationsModelManagerTest, PageTopics) {
  SetupPageTopicsV2ModelExecutor();

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  std::vector<BatchAnnotationResult> result;
  BatchAnnotationCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop, &result);

  model_manager()->Annotate(std::move(callback), {"input"},
                            AnnotationType::kPageTopics);
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageTopicsV2",
      ExecutionStatus::kErrorModelFileNotValid, 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize.PageTopics",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.PageTopics", false,
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime.PageTopics",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime.PageTopics", 1);

  EXPECT_TRUE(model_observer_tracker()->DidRegisterForTarget(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAGE_TOPICS_V2, nullptr));

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].input(), "input");
  EXPECT_EQ(result[0].type(), AnnotationType::kPageTopics);
  EXPECT_EQ(result[0].topics(), absl::nullopt);
  EXPECT_EQ(result[0].entities(), absl::nullopt);
  EXPECT_EQ(result[0].visibility_score(), absl::nullopt);
}

TEST_F(PageContentAnnotationsModelManagerTest, PageTopicsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kPageTopicsBatchAnnotations);
  SetupPageTopicsV2ModelExecutor();

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  std::vector<BatchAnnotationResult> result;
  BatchAnnotationCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop, &result);

  model_manager()->Annotate(std::move(callback), {"input"},
                            AnnotationType::kPageTopics);
  run_loop.Run();

  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageTopicsV2", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize.PageTopics",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.PageTopics", false,
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime.PageTopics",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime.PageTopics", 1);

  EXPECT_FALSE(model_observer_tracker()->DidRegisterForTarget(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAGE_TOPICS_V2, nullptr));

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].input(), "input");
  EXPECT_EQ(result[0].type(), AnnotationType::kPageTopics);
  EXPECT_EQ(result[0].topics(), absl::nullopt);
  EXPECT_EQ(result[0].entities(), absl::nullopt);
  EXPECT_EQ(result[0].visibility_score(), absl::nullopt);
}

TEST_F(PageContentAnnotationsModelManagerTest, PageEntities) {
  std::vector<ScoredEntityMetadata> input1_entities = {
      ScoredEntityMetadata(0.5, EntityMetadata("cat", "cat", {})),
      ScoredEntityMetadata(0.6, EntityMetadata("dog", "dog", {})),
  };
  std::vector<ScoredEntityMetadata> input2_entities = {
      ScoredEntityMetadata(0.7, EntityMetadata("fish", "fish", {})),
  };
  SetPageEntitiesModelHandler(
      {
          {"input1", input1_entities},
          {"input2", input2_entities},
          {"other input",
           {
               ScoredEntityMetadata(0.7, EntityMetadata("other", "other", {})),
           }},
      },
      /*entity_metadata=*/{});

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  std::vector<BatchAnnotationResult> result;
  BatchAnnotationCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop, &result);

  model_manager()->Annotate(std::move(callback), {"input1", "input2"},
                            AnnotationType::kPageEntities);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize."
      "PageEntities",
      2, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.PageEntities",
      true, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime.PageEntities",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime.PageEntities",
      1);

  run_loop.Run();

  ASSERT_EQ(result.size(), 2U);

  EXPECT_EQ(result[0].input(), "input1");
  EXPECT_EQ(result[0].topics(), absl::nullopt);
  EXPECT_EQ(result[0].entities(), absl::make_optional(input1_entities));
  EXPECT_EQ(result[0].visibility_score(), absl::nullopt);
  EXPECT_EQ(result[0].type(), AnnotationType::kPageEntities);

  EXPECT_EQ(result[1].input(), "input2");
  EXPECT_EQ(result[1].topics(), absl::nullopt);
  EXPECT_EQ(result[1].entities(), absl::make_optional(input2_entities));
  EXPECT_EQ(result[1].visibility_score(), absl::nullopt);
  EXPECT_EQ(result[1].type(), AnnotationType::kPageEntities);
}

TEST_F(PageContentAnnotationsModelManagerTest, PageVisibility) {
  base::HistogramTester histogram_tester;
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.set_version(123);
  page_topics_model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("DO NOT EVALUATE");
  page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  base::RunLoop run_loop;
  std::vector<BatchAnnotationResult> result;
  BatchAnnotationCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop, &result);

  model_manager()->Annotate(std::move(callback), {"input"},
                            AnnotationType::kContentVisibility);
  run_loop.Run();

  EXPECT_TRUE(model_observer_tracker()->DidRegisterForTarget(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAGE_VISIBILITY, nullptr));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize."
      "ContentVisibility",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.ContentVisibility",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime."
      "ContentVisibility",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime."
      "ContentVisibility",
      1);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].input(), "input");
  EXPECT_EQ(result[0].topics(), absl::nullopt);
  EXPECT_EQ(result[0].entities(), absl::nullopt);
  EXPECT_EQ(result[0].visibility_score(), absl::nullopt);
}

TEST_F(PageContentAnnotationsModelManagerTest, PageVisibilityDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kPageVisibilityBatchAnnotations);

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.set_version(123);
  page_topics_model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("DO NOT EVALUATE");
  page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  base::RunLoop run_loop;
  std::vector<BatchAnnotationResult> result;
  BatchAnnotationCallback callback = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop, &result);

  model_manager()->Annotate(std::move(callback), {"input"},
                            AnnotationType::kContentVisibility);
  run_loop.Run();

  EXPECT_FALSE(model_observer_tracker()->DidRegisterForTarget(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAGE_VISIBILITY, nullptr));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize."
      "ContentVisibility",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.ContentVisibility",
      false, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime."
      "ContentVisibility",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime."
      "ContentVisibility",
      1);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].input(), "input");
  EXPECT_EQ(result[0].topics(), absl::nullopt);
  EXPECT_EQ(result[0].entities(), absl::nullopt);
  EXPECT_EQ(result[0].visibility_score(), absl::nullopt);
}

TEST_F(PageContentAnnotationsModelManagerTest, CalledTwice) {
  SetupPageTopicsV2ModelExecutor();

  base::HistogramTester histogram_tester;

  base::RunLoop run_loop1;
  std::vector<BatchAnnotationResult> result1;
  BatchAnnotationCallback callback1 = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop1, &result1);

  model_manager()->Annotate(std::move(callback1), {"input1"},
                            AnnotationType::kPageTopics);

  base::RunLoop run_loop2;
  std::vector<BatchAnnotationResult> result2;
  BatchAnnotationCallback callback2 = base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<BatchAnnotationResult>* out_result,
         const std::vector<BatchAnnotationResult>& in_result) {
        *out_result = in_result;
        run_loop->Quit();
      },
      &run_loop2, &result2);

  model_manager()->Annotate(std::move(callback2), {"input2"},
                            AnnotationType::kPageTopics);

  run_loop1.Run();
  run_loop2.Run();

  EXPECT_TRUE(model_observer_tracker()->DidRegisterForTarget(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAGE_TOPICS_V2, nullptr));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchRequestedSize.PageTopics",
      1, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.BatchSuccess.PageTopics", false,
      2);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobExecutionTime.PageTopics",
      2);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotations.JobScheduleTime.PageTopics", 2);

  // The model should have only been loaded once and then used for both jobs.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ModelAvailableToLoad.PageTopicsV2", true,
      2);

  ASSERT_EQ(result1.size(), 1U);
  EXPECT_EQ(result1[0].input(), "input1");
  EXPECT_EQ(result1[0].type(), AnnotationType::kPageTopics);
  EXPECT_EQ(result1[0].topics(), absl::nullopt);
  EXPECT_EQ(result1[0].entities(), absl::nullopt);
  EXPECT_EQ(result1[0].visibility_score(), absl::nullopt);
  ASSERT_EQ(result2.size(), 1U);
  EXPECT_EQ(result2[0].input(), "input2");
  EXPECT_EQ(result2[0].type(), AnnotationType::kPageTopics);
  EXPECT_EQ(result2[0].topics(), absl::nullopt);
  EXPECT_EQ(result2[0].entities(), absl::nullopt);
  EXPECT_EQ(result2[0].visibility_score(), absl::nullopt);
}

TEST_F(PageContentAnnotationsModelManagerTest, GetModelInfoForType) {
  EXPECT_FALSE(
      model_manager()->GetModelInfoForType(AnnotationType::kPageTopics));
  EXPECT_FALSE(
      model_manager()->GetModelInfoForType(AnnotationType::kContentVisibility));

  SetupPageTopicsV2ModelExecutor();
  EXPECT_TRUE(
      model_manager()->GetModelInfoForType(AnnotationType::kPageTopics));

  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.set_version(123);
  page_topics_model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("DO NOT EVALUATE");
  page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  EXPECT_TRUE(
      model_manager()->GetModelInfoForType(AnnotationType::kContentVisibility));
}

TEST_F(PageContentAnnotationsModelManagerTest,
       NotifyWhenModelAvailable_TopicsOnly) {
  SetupPageTopicsV2ModelExecutor();

  base::RunLoop topics_run_loop;
  bool topics_callback_success = false;

  model_manager()->RequestAndNotifyWhenModelAvailable(
      AnnotationType::kPageTopics,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* out_success, bool success) {
            *out_success = success;
            run_loop->Quit();
          },
          &topics_run_loop, &topics_callback_success));

  topics_run_loop.Run();

  EXPECT_TRUE(topics_callback_success);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       NotifyWhenModelAvailable_VisibilityOnly) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.set_version(123);
  page_topics_model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("DO NOT EVALUATE");
  page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  base::RunLoop visibility_run_loop;
  bool visibility_callback_success = false;

  model_manager()->RequestAndNotifyWhenModelAvailable(
      AnnotationType::kContentVisibility,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* out_success, bool success) {
            *out_success = success;
            run_loop->Quit();
          },
          &visibility_run_loop, &visibility_callback_success));

  visibility_run_loop.Run();

  EXPECT_TRUE(visibility_callback_success);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       NotifyWhenModelAvailable_EntitiesOnly) {
  SetPageEntitiesModelHandler(/*entries=*/{}, /*entity_metadata=*/{});

  base::RunLoop run_loop;
  bool success = false;

  model_manager()->RequestAndNotifyWhenModelAvailable(
      AnnotationType::kPageEntities,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* out_success, bool success) {
            *out_success = success;
            run_loop->Quit();
          },
          &run_loop, &success));

  run_loop.Run();

  EXPECT_TRUE(success);
}

TEST_F(PageContentAnnotationsModelManagerTest, NotifyWhenModelAvailable_Both) {
  proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.set_version(123);
  page_topics_model_metadata.mutable_output_postprocessing_params()
      ->mutable_visibility_params()
      ->set_category_name("DO NOT EVALUATE");
  page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
  SendPageVisibilityModelToExecutor(any_metadata);

  SetupPageTopicsV2ModelExecutor();

  base::RunLoop topics_run_loop;
  base::RunLoop visibility_run_loop;
  bool topics_callback_success = false;
  bool visibility_callback_success = false;

  model_manager()->RequestAndNotifyWhenModelAvailable(
      AnnotationType::kPageTopics,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* out_success, bool success) {
            *out_success = success;
            run_loop->Quit();
          },
          &topics_run_loop, &topics_callback_success));
  model_manager()->RequestAndNotifyWhenModelAvailable(
      AnnotationType::kContentVisibility,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* out_success, bool success) {
            *out_success = success;
            run_loop->Quit();
          },
          &visibility_run_loop, &visibility_callback_success));

  topics_run_loop.Run();
  visibility_run_loop.Run();

  EXPECT_TRUE(topics_callback_success);
  EXPECT_TRUE(visibility_callback_success);
}

TEST_F(PageContentAnnotationsModelManagerTest,
       GetMetadataForEntityIdEntitiesAnnotatorInitialized) {
  EntityMetadata entity_metadata;
  entity_metadata.human_readable_name = "entity1";
  entity_metadata.human_readable_categories = {
      {"category1", 0.5},
  };
  SetPageEntitiesModelHandler(/*entries=*/{}, {
                                                  {"entity1", entity_metadata},
                                              });
  EXPECT_TRUE(GetMetadataForEntityId("entity1").has_value());
}

}  // namespace optimization_guide
