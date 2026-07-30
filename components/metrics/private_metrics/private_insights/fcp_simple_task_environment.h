// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_SIMPLE_TASK_ENVIRONMENT_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_SIMPLE_TASK_ENVIRONMENT_H_

#include "third_party/federated_compute/src/fcp/client/example_query_result.pb.h"
#include "third_party/federated_compute/src/fcp/client/simple_task_environment.h"

namespace private_insights {

class FcpSimpleTaskEnvironment : public fcp::client::SimpleTaskEnvironment {
 public:
  FcpSimpleTaskEnvironment(std::string base_dir,
                           std::string cache_dir,
                           fcp::client::ExampleQueryResult result);
  ~FcpSimpleTaskEnvironment() override;

  FcpSimpleTaskEnvironment(const FcpSimpleTaskEnvironment&) = delete;
  FcpSimpleTaskEnvironment& operator=(const FcpSimpleTaskEnvironment&) = delete;

  std::string GetBaseDir() override;
  std::string GetCacheDir() override;

  absl::StatusOr<std::unique_ptr<fcp::client::ExampleIterator>>  // nocheck
  CreateExampleIterator(
      const google::internal::federated::plan::ExampleSelector&
          example_selector) override;

  bool TrainingConditionsSatisfied() override;

 private:
  std::string base_dir_;
  std::string cache_dir_;

  fcp::client::ExampleQueryResult result_;
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_SIMPLE_TASK_ENVIRONMENT_H_
