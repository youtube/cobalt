// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/model_executor.h"

namespace optimization_guide {

class TestModelExecutor
    : public ModelExecutor<std::vector<float>, const std::vector<float>&> {
 public:
  TestModelExecutor() = default;
  ~TestModelExecutor() override = default;

  void InitializeAndMoveToExecutionThread(
      absl::optional<base::TimeDelta>,
      proto::OptimizationTarget,
      scoped_refptr<base::SequencedTaskRunner>,
      scoped_refptr<base::SequencedTaskRunner>) override {}

  void UpdateModelFile(const base::FilePath&) override {}

  void UnloadModel() override {}

  void SetShouldUnloadModelOnComplete(bool should_auto_unload) override {}

  using ExecutionCallback =
      base::OnceCallback<void(const absl::optional<std::vector<float>>&)>;
  void SendForExecution(ExecutionCallback callback_on_complete,
                        base::TimeTicks start_time,
                        const std::vector<float>& args) override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_TEST_MODEL_EXECUTOR_H_
