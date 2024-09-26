// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_EXECUTION_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_EXECUTION_SERVICE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "components/segmentation_platform/internal/data_collection/training_data_collector.h"
#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

class PrefService;

namespace segmentation_platform {
namespace processing {
class FeatureListQueryProcessor;
class InputDelegateHolder;
}

struct Config;
struct PlatformOptions;
class ModelExecutor;
class ModelProviderFactory;
class SignalHandler;
class StorageService;

// Handles feature processing and model execution.
class ExecutionService {
 public:
  ExecutionService();
  ~ExecutionService();

  ExecutionService(const ExecutionService&) = delete;
  ExecutionService& operator=(const ExecutionService&) = delete;

  void InitForTesting(
      std::unique_ptr<processing::FeatureListQueryProcessor> feature_processor,
      std::unique_ptr<ModelExecutor> executor,
      std::unique_ptr<ModelExecutionScheduler> scheduler,
      std::unique_ptr<ModelExecutionManager> execution_manager);

  void Initialize(
      StorageService* storage_service,
      SignalHandler* signal_handler,
      base::Clock* clock,
      ModelExecutionManager::SegmentationModelUpdatedCallback callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::flat_set<SegmentId>& all_segment_ids,
      ModelProviderFactory* model_provider_factory,
      std::vector<ModelExecutionScheduler::Observer*>&& observers,
      const PlatformOptions& platform_options,
      std::unique_ptr<processing::InputDelegateHolder> input_delegate_holder,
      std::vector<std::unique_ptr<Config>>* configs,
      PrefService* profile_prefs);

  // Returns the training data collector.
  TrainingDataCollector* training_data_collector() {
    return training_data_collector_.get();
  }

  // Called whenever a new or updated model is available. Must be a valid
  // SegmentInfo with valid metadata and features.
  void OnNewModelInfoReady(const proto::SegmentInfo& segment_info);

  // Gets the model provider for execution.
  ModelProvider* GetModelProvider(SegmentId segment_id);

  void RequestModelExecution(std::unique_ptr<ExecutionRequest> request);

  void OverwriteModelExecutionResult(
      proto::SegmentId segment_id,
      const std::pair<float, ModelExecutionStatus>& result);

  // Refreshes model results for all eligible models.
  void RefreshModelResults();

  // Executes daily maintenance and collection tasks.
  void RunDailyTasks(bool is_startup);

 private:
  raw_ptr<StorageService> storage_service_{};

  // Training/inference input data generation.
  std::unique_ptr<processing::FeatureListQueryProcessor>
      feature_list_query_processor_;

  // Traing data collection logic.
  std::unique_ptr<TrainingDataCollector> training_data_collector_;

  // Utility to execute model and return result.
  std::unique_ptr<ModelExecutor> model_executor_;

  // Model execution.
  std::unique_ptr<ModelExecutionManager> model_execution_manager_;

  // Model execution scheduling logic.
  std::unique_ptr<ModelExecutionScheduler> model_execution_scheduler_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SCHEDULER_EXECUTION_SERVICE_H_
